#include "network.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTimer>
#include <QCoreApplication>
#include <QSysInfo>
#include <QUrlQuery>
#include <algorithm>
#include <limits>

namespace Studio {
Network::Network(CacheStore *cache, QObject *parent, QNetworkAccessManager *transport, const NetworkPolicy &policy)
    : QObject(parent), m_cache(cache), m_transport(transport ? transport : &m_manager), m_policy(policy) { m_transport->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy); }
Network::~Network() {
    for (const auto &file : m_cachedPins) m_cache->unpin(file);
    for (const auto &job : m_jobs) if (job->reply) {
        disconnect(job->reply, nullptr, this, nullptr);
        job->reply->abort();
    }
}
bool Network::allowed(const QUrl &url, bool api) {
    if (!url.isValid() || url.scheme() != "https" || !url.userInfo().isEmpty() || (url.port() != -1 && url.port() != 443)) return false;
    const auto host = url.host().toLower();
    if (api) return (host == "www.steamgriddb.com" || host == "steamgriddb.com") && url.path().startsWith("/api/");
    return host == "steamgriddb.com" || host.endsWith(".steamgriddb.com");
}
void Network::setApiKey(const QString &key) { m_key = key.trimmed(); }
void Network::deliverReady(quint64 token, const QString &file, bool cached) { if (m_live.remove(token)) emit ready(token, file, cached); }
void Network::deliverFailure(quint64 token, const QString &message, int status) { if (m_live.remove(token)) emit failed(token, message, status); }
quint64 Network::fetch(const QUrl &url, const QString &kind) {
    const auto token = ++m_next; m_live.insert(token); const bool api = kind == "api";
    if (!allowed(url, api)) { QTimer::singleShot(0, this, [this, token] { deliverFailure(token, "Dirección no permitida. Solo se admite HTTPS de SteamGridDB."); }); return token; }
    if (api && m_key.isEmpty()) { QTimer::singleShot(0, this, [this, token] { deliverFailure(token, "Conecta SteamGridDB para buscar imágenes nuevas."); }); return token; }
    const auto entry = m_cache->lookup(url, api ? (url.path().contains("/games/") ? 86400 : 600) : 86400 * 30, api ? 86400 : 86400 * 90);
    if (entry.valid()) {
        m_cache->pin(entry.file);
        m_cachedPins[token] = entry.file;
        QTimer::singleShot(0, this, [this, token, file = entry.file] { deliverReady(token, file, true); if (m_cachedPins.remove(token)) m_cache->unpin(file); });
        if (entry.fresh) return token;
    }
    const qint64 delay = m_backoffHosts.value(url.host()) - QDateTime::currentMSecsSinceEpoch();
    if (delay > 0) {
        if (!entry.valid()) QTimer::singleShot(0, this, [this, token, delay] { deliverFailure(token, QString("El servidor pide esperar %1 s antes de otra consulta.").arg((delay + 999) / 1000), 429); });
        return token;
    }
    const auto key = url.toString(QUrl::FullyEncoded) + "|" + kind;
    auto existing = m_jobs.value(key);
    if (existing) { if (!entry.valid()) existing->tokens.append(token); return token; }
    auto job = QSharedPointer<Job>::create(); job->original = job->current = url; job->kind = kind; job->key = key;
    if (!entry.valid()) job->tokens.append(token);
    m_jobs.insert(key, job); m_queue.append(job); pump(); return token;
}
void Network::cancel(quint64 token) {
    if (!m_live.remove(token)) return;
    if (m_cachedPins.contains(token)) m_cache->unpin(m_cachedPins.take(token));
    QSharedPointer<Job> abandoned;
    for (const auto &job : m_jobs.values()) {
        if (!job->tokens.removeOne(token)) continue;
        if (job->tokens.isEmpty()) {
            job->cancelled = true; abandoned = job;
            m_queue.removeAll(job); m_jobs.remove(job->key);
        }
        break;
    }
    emit cancelled(token);
    if (abandoned && abandoned->reply) abandoned->reply->abort();
    pump();
}
void Network::cancelAll() {
    m_cancelling = true;
    const auto tokens = m_live.values(); for (quint64 token : tokens) cancel(token);
    const auto jobs = m_jobs.values();
    for (const auto &job : jobs) if (job->tokens.isEmpty()) {
        job->cancelled = true; m_queue.removeAll(job); m_jobs.remove(job->key);
        if (job->reply) job->reply->abort();
    }
    m_cancelling = false; pump();
}
void Network::pump() {
    if (m_cancelling) return;
    std::stable_sort(m_queue.begin(), m_queue.end(), [](const auto &a, const auto &b) {
        auto rank = [](const QString &kind) { return kind == "api" ? 0 : kind == "original" ? 1 : 2; };
        return rank(a->kind) < rank(b->kind);
    });
    while (m_active < 4 && !m_queue.isEmpty()) {
        int chosen = -1;
        for (int i = 0; i < m_queue.size(); ++i) if (m_queue[i]->kind == "api" || m_images < 2) { chosen = i; break; }
        if (chosen < 0) break;
        auto job = m_queue.takeAt(chosen); start(job);
    }
    emit activity(m_active, m_queue.size());
}
void Network::start(const QSharedPointer<Job> &job) {
    const qint64 delay = m_backoffHosts.value(job->current.host()) - QDateTime::currentMSecsSinceEpoch();
    if (delay > 0) {
        m_jobs.remove(job->key);
        QTimer::singleShot(0, this, [this, job, delay] {
            for (quint64 token : job->tokens) deliverFailure(token, QString("El servidor pide esperar %1 s antes de otra consulta.").arg((delay + 999) / 1000), 429);
        });
        return;
    }
    job->active = true; job->data.clear(); job->oversized = false; job->timedOut = false; job->timeoutMessage.clear(); ++m_active; if (job->kind != "api") ++m_images;
    const bool original = job->kind == "original";
    const int connectionMs = qMax(1, original ? m_policy.originalConnectionMs : m_policy.connectionMs);
    const int transferMs = qMax(1, original ? m_policy.originalTransferMs : m_policy.transferMs);
    const int deadlineMs = qMax(1, original ? m_policy.originalDeadlineMs : m_policy.deadlineMs);
    QNetworkRequest request(job->current); request.setRawHeader("User-Agent", "Steam-Art-Studio/" + QCoreApplication::applicationVersion().toLatin1() + " (" + QSysInfo::kernelType().toLatin1() + "; Qt)");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(transferMs);
    if (job->kind == "api") { request.setRawHeader("Accept", "application/json"); request.setRawHeader("Authorization", "Bearer " + m_key.toUtf8()); }
    auto reply = m_transport->get(request); job->reply = reply;
    reply->setReadBufferSize(256 * 1024);
    const auto timeout = [job, reply](const QString &message) {
        if (job->timedOut || reply->isFinished()) return;
        job->timedOut = true; job->timeoutMessage = message; reply->abort();
    };
    auto connectTimer = new QTimer(reply); connectTimer->setSingleShot(true); connectTimer->start(connectionMs);
    connect(connectTimer, &QTimer::timeout, this, [timeout, connectionMs] {
        timeout(QString("Se agotó el tiempo de espera al iniciar la conexión (%1 s). Puedes reintentar la descarga desde Descargas.").arg(connectionMs / 1000.));
    });
    connect(reply, &QNetworkReply::encrypted, connectTimer, &QTimer::stop);
    connect(reply, &QNetworkReply::metaDataChanged, connectTimer, &QTimer::stop);
    auto idle = new QTimer(reply); idle->setSingleShot(true); idle->start(transferMs);
    connect(idle, &QTimer::timeout, this, [timeout, transferMs] {
        timeout(QString("Se agotó el tiempo de espera: no se recibieron datos durante %1 s. Puedes reintentar la descarga desde Descargas.").arg(transferMs / 1000.));
    });
    auto deadline = new QTimer(reply); deadline->setSingleShot(true); deadline->start(deadlineMs);
    connect(deadline, &QTimer::timeout, this, [timeout, deadlineMs] {
        timeout(QString("Se agotó el tiempo de espera total de la descarga (%1 s). Puedes reintentar o descargar el archivo en el navegador y usar Imagen local. No se ha modificado Steam.").arg(deadlineMs / 1000.));
    });
    connect(reply, &QIODevice::readyRead, this, [job, reply, connectTimer, idle, transferMs] {
        const qint64 maximum = job->kind == "api" ? 2 * 1024 * 1024 : job->kind == "original" ? 64 * 1024 * 1024 : 8 * 1024 * 1024;
        const auto bytes = reply->readAll();
        if (!bytes.isEmpty()) { connectTimer->stop(); idle->start(transferMs); }
        job->data.append(bytes); if (job->data.size() > maximum) { job->oversized = true; reply->abort(); }
    });
    connect(reply, &QNetworkReply::finished, this, [this, job] { finish(job); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, job](qint64 received, qint64 total) { for (quint64 token : job->tokens) if (m_live.contains(token)) emit progress(token, received, total); });
}
void Network::finish(const QSharedPointer<Job> &job) {
    auto reply = job->reply; job->active = false; --m_active; if (job->kind != "api") --m_images;
    for (auto *timer : reply->findChildren<QTimer *>()) timer->stop();
    if (job->cancelled) { job->data.clear(); reply->deleteLater(); job->reply = nullptr; pump(); return; }
    if (!job->timedOut && reply->error() == QNetworkReply::TimeoutError) {
        job->timedOut = true;
        job->timeoutMessage = "Se agotó el tiempo de espera de la transferencia. Puedes reintentar la descarga desde Descargas.";
    }
    if (reply->isOpen()) job->data.append(reply->readAll());
    const qint64 maximum = job->kind == "api" ? 2 * 1024 * 1024 : job->kind == "original" ? 64 * 1024 * 1024 : 8 * 1024 * 1024;
    if (job->data.size() > maximum) job->oversized = true;
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 429) {
        const QByteArray header = reply->rawHeader("Retry-After"); bool ok = false; qint64 seconds = header.toLongLong(&ok);
        if (!ok) { const QDateTime until = QDateTime::fromString(QString::fromLatin1(header), Qt::RFC2822Date); seconds = until.isValid() ? QDateTime::currentDateTimeUtc().secsTo(until) : 30; }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 maximum = (std::numeric_limits<qint64>::max() - now) / 1000;
        const qint64 until = now + qBound<qint64>(1LL, seconds, maximum) * 1000;
        m_backoffHosts[job->current.host()] = until; m_backoffHosts[job->original.host()] = until;
    }
    const auto redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (status >= 300 && status < 400 && redirect.isValid() && job->redirects++ < 3) {
        auto next = job->current.resolved(redirect);
        if (allowed(next, job->kind == "api")) { job->current = next; reply->deleteLater(); job->reply = nullptr; m_queue.prepend(job); pump(); return; }
    }
    // Rate limits are surfaced to the user, never retried before Retry-After.
    const bool retryable = !job->oversized && !job->timedOut && (status == 408 || status >= 500 || (status == 0 && reply->error() != QNetworkReply::NoError));
    if (retryable && job->attempts++ < 1 && !job->tokens.isEmpty()) {
        const int wait = m_policy.retryMs;
        reply->deleteLater(); job->reply = nullptr;
        QTimer::singleShot(wait, this, [this, job] { if (m_jobs.value(job->key) == job) { m_queue.append(job); pump(); } }); pump(); return;
    }
    QString error;
    QString mime = reply->header(QNetworkRequest::ContentTypeHeader).toString().section(';', 0, 0).toLower();
    if (job->oversized) error = "La descarga supera el tamaño máximo permitido.";
    else if (job->timedOut) error = job->timeoutMessage;
    else if (status == 401 || status == 403) error = "SteamGridDB ha rechazado la consulta. Comprueba la clave o inténtalo más tarde.";
    else if (status == 429) error = "SteamGridDB ha limitado las consultas. Espera antes de volver a intentarlo; no se reintentará automáticamente.";
    else if (reply->error() != QNetworkReply::NoError || status != 200) error = status == 0 ? "No se pudo establecer una conexión segura con el servidor. Comprueba la conexión; la biblioteca local sigue disponible." : QString("No se pudo completar la descarga (HTTP %1). La biblioteca local sigue disponible.").arg(status);
    else if (job->kind == "api") {
        const auto object = QJsonDocument::fromJson(job->data).object(); if (!object.value("success").toBool()) error = "SteamGridDB devolvió una respuesta no válida.";
    } else if (!mime.startsWith("image/") && !mime.startsWith("video/") && mime != "application/octet-stream") error = "El servidor no ha devuelto una imagen o animación.";
    QString path;
    if (error.isEmpty()) path = m_cache->store(job->original, job->data, mime, &error);
    const auto tokens = job->tokens; if (m_jobs.value(job->key) == job) m_jobs.remove(job->key); reply->deleteLater(); job->reply = nullptr; job->data.clear();
    if (!path.isEmpty()) m_cache->pin(path);
    for (auto token : tokens) { if (!path.isEmpty()) deliverReady(token, path, false); else deliverFailure(token, error.isEmpty() ? "No se pudo guardar la descarga." : error, status); }
    if (!path.isEmpty()) m_cache->unpin(path);
    pump();
}
ArtService::ArtService(Network *network, QObject *parent) : QObject(parent), m_network(network) {
    connect(network, &Network::ready, this, &ArtService::received);
    connect(network, &Network::failed, this, [this](quint64 token, const QString &message, int status) {
        auto it = m_requests.find(token); if (it == m_requests.end()) return; auto context = it.value(); m_requests.erase(it);
        if (context.generation != m_generation) return;
        if (context.action == "lookup" && status == 404) {
            const auto path = QByteArray("https://www.steamgriddb.com/api/v2/search/autocomplete/") + QUrl::toPercentEncoding(m_game.name);
            request(QUrl::fromEncoded(path), "search"); emit loading("Buscando una coincidencia por nombre…");
        } else emit error(message);
    });
}
void ArtService::invalidate() { ++m_generation; auto tokens = m_requests.keys(); m_requests.clear(); for (auto token : tokens) m_network->cancel(token); }
void ArtService::setMapping(const QString &gameKey, qint64 sgdbId) { if (sgdbId > 0) m_gameIds[gameKey] = sgdbId; }
void ArtService::search(const Game &game, const QString &type, bool statics, bool animations, bool adult, int page) {
    invalidate(); m_game = game; m_type = type; m_static = statics; m_animated = animations && type != "icon"; m_adult = adult; m_page = page;
    if (!artTypes().contains(type)) { emit error("Tipo de imagen no válido."); return; }
    if (!m_static && !m_animated) { emit results({}, page, 0, true); return; }
    if (!m_network->hasApiKey()) { emit error("Conecta SteamGridDB para buscar carátulas. Tu biblioteca local funciona sin clave."); return; }
    emit loading("Buscando arte para «" + game.name + "»…");
    if (m_gameIds.contains(game.key())) { requestArt(m_gameIds.value(game.key())); return; }
    if (!game.external) request(QUrl("https://www.steamgriddb.com/api/v2/games/steam/" + QString::number(game.appId)), "lookup");
    else request(QUrl::fromEncoded(QByteArray("https://www.steamgriddb.com/api/v2/search/autocomplete/") + QUrl::toPercentEncoding(game.name)), "search");
}
void ArtService::request(const QUrl &url, const QString &action) { auto token = m_network->fetch(url, "api"); m_requests.insert(token, {m_generation, action}); }
void ArtService::requestArt(qint64 id) {
    m_gameIds[m_game.key()] = id;
    request(artworkUrl(m_type, id, m_static, m_animated, m_adult, m_page), "art");
}
QUrl ArtService::artworkUrl(const QString &type, qint64 id, bool statics, bool animations, bool adult, int page) {
    if (!artTypes().contains(type) || id <= 0 || page < 0) return {};
    const QString endpoint = type == "hero" ? "heroes" : type == "logo" ? "logos" : type == "icon" ? "icons" : "grids";
    QUrl url("https://www.steamgriddb.com/api/v2/" + endpoint + "/game/" + QString::number(id)); QUrlQuery query;
    if (type == "capsule") query.addQueryItem("dimensions", "600x900,342x482,660x930,512x512,1024x1024");
    if (type == "wide") query.addQueryItem("dimensions", "460x215,920x430,512x512,1024x1024");
    if (type == "icon") query.addQueryItem("mimes", "image/png,image/vnd.microsoft.icon");
    query.addQueryItem("nsfw", adult ? "any" : "false"); query.addQueryItem("epilepsy", "false");
    QStringList types; if (statics) types << "static"; if (animations && type != "icon") types << "animated";
    query.addQueryItem("types", types.join(',')); query.addQueryItem("page", QString::number(page)); url.setQuery(query); return url;
}
void ArtService::received(quint64 token, const QString &file, bool cached) {
    auto it = m_requests.find(token); if (it == m_requests.end()) return; auto context = it.value(); m_requests.erase(it);
    if (context.generation != m_generation) return;
    QFile input(file); if (!input.open(QIODevice::ReadOnly) || input.size() > 2 * 1024 * 1024) { emit error("No se pudo leer la respuesta de SteamGridDB."); return; }
    const auto object = QJsonDocument::fromJson(input.readAll()).object();
    if (context.action == "lookup") {
        const auto id = static_cast<qint64>(object.value("data").toObject().value("id").toDouble());
        if (id) requestArt(id); else emit error("No se ha encontrado este juego en SteamGridDB.");
    } else if (context.action == "search") {
        QJsonArray exact;
        for (const auto &item : object.value("data").toArray()) if (item.toObject().value("name").toString().trimmed().compare(m_game.name.trimmed(), Qt::CaseInsensitive) == 0) exact.append(item);
        if (exact.size() != 1) { emit error("No hay una coincidencia única. Usa «Buscar título…» para elegir el juego correcto o prepara una imagen local."); return; }
        requestArt(static_cast<qint64>(exact.first().toObject().value("id").toDouble()));
    } else {
        QVector<Artwork> artworks;
        for (const auto &item : object.value("data").toArray()) {
            const auto data = item.toObject(); Artwork art; art.id = QString::number(static_cast<qint64>(data.value("id").toDouble()));
            art.author = data.value("author").toObject().value("name").toString(); art.type = m_type;
            art.thumbnail = QUrl(data.value("thumb").toString()); art.original = QUrl(data.value("url").toString()); art.mime = data.value("mime").toString();
            if (m_type == "icon" && !Network::allowed(art.thumbnail, false)) art.thumbnail = art.original;
            art.width = data.value("width").toInt(); art.height = data.value("height").toInt(); art.adult = data.value("nsfw").toBool();
            art.animated = data.value("type").toString() == "animated" || art.thumbnail.path().endsWith(".webm");
            if (m_type == "icon" && art.animated) continue;
            if (Network::allowed(art.thumbnail, false) && Network::allowed(art.original, false)) artworks.append(art);
        }
        emit results(artworks, object.value("page").toInt(m_page), object.value("total").toInt(artworks.size()), cached);
    }
}
}
