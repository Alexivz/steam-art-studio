#include "draft.h"
#include "cache.h"
#include "transactions.h"
#include "platform.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <cmath>

namespace Studio {
namespace {
QByteArray readDraft(const QString &path) {
    QFile file(path); if (Platform::isLink(QFileInfo(path)) || !file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) return {};
    return file.readAll();
}
bool validDocument(const QByteArray &data) {
    QJsonParseError error; const auto doc = QJsonDocument::fromJson(data, &error);
    return error.error == QJsonParseError::NoError && doc.isArray();
}
QString resolvedPath(QString path) {
    path = QDir::cleanPath(QDir(path).absolutePath()); QStringList suffix;
    while (!QFileInfo::exists(path)) { const QFileInfo info(path); suffix.prepend(info.fileName()); const QString parent = info.absolutePath(); if (parent == path) return {}; path = parent; }
    const QString base = QFileInfo(path).canonicalFilePath(); return base.isEmpty() ? QString() : QDir::cleanPath(base + "/" + suffix.join('/'));
}
QJsonObject encodeGame(const Game &g) {
    return {{"appid", static_cast<double>(g.appId)}, {"name", g.name}, {"root", g.steamRoot}, {"user", g.userId}, {"external", g.external}, {"icon", g.icon}, {"iconHash", g.iconHash}};
}
}
DraftStore::DraftStore(const QString &data, const QString &cache, bool demo) : m_data(data), m_cache(cache), m_demo(demo) {}
bool DraftStore::separateDirectories(const QString &data, const QString &cache) {
    const QString a = resolvedPath(data), b = resolvedPath(cache);
    return !a.isEmpty() && !b.isEmpty() && !Platform::sameOrBelow(a, b) && !Platform::sameOrBelow(b, a);
}
bool DraftStore::save(const QVector<PendingChange> &changes, QString *error) const {
    QJsonArray entries;
    for (const auto &change : changes) entries.append(QJsonObject{{"game", encodeGame(change.game)}, {"type", change.type}, {"file", change.file}, {"description", change.description}, {"animated", change.animated}});
    const auto bytes = QJsonDocument(entries).toJson(QJsonDocument::Compact);
    if (changes.size() > 100 || bytes.size() > 2 * 1024 * 1024) { if (error) *error = "La cola supera el límite de persistencia."; return false; }
    const QString path = m_data + "/draft.json";
    if (QFileInfo::exists(path)) {
        const QByteArray previous = readDraft(path);
        if (!validDocument(previous)) { if (error) *error = "La cola en disco está dañada. Se conserva sin sobrescribir; vuelve a abrir para recuperar su copia anterior."; return false; }
        if (!CacheStore::atomicWrite(m_data + "/draft.previous.json", previous, error)) return false;
    }
    return CacheStore::atomicWrite(path, bytes, error);
}
QStringList DraftStore::previousOriginals() const {
    const auto bytes = readDraft(m_data + "/draft.previous.json");
    const QString cache = QFileInfo(m_cache).canonicalFilePath();
    if (!validDocument(bytes) || cache.isEmpty()) return {};
    QSet<QString> paths;
    const auto entries = QJsonDocument::fromJson(bytes).array();
    if (entries.size() > 100) return {};
    for (const auto &entry : entries) {
        const QFileInfo file(entry.toObject().value("file").toString());
        if (!Platform::isLink(file) && file.isFile() && Platform::sameOrBelow(file.canonicalFilePath(), cache)) paths.insert(file.canonicalFilePath());
    }
    return paths.values();
}
DraftResult DraftStore::load() const {
    DraftResult result; const QString path = m_data + "/draft.json";
    QByteArray bytes = readDraft(path);
    if (!QFileInfo::exists(path) && !QFileInfo::exists(m_data + "/draft.previous.json")) return result;
    if (!validDocument(bytes)) {
        if (QFileInfo::exists(path)) {
            const QString preserved = m_data + "/draft.corrupt-" + QUuid::createUuid().toString(QUuid::Id128) + ".json";
            if (!QFile::rename(path, preserved)) { result.warning = "No se puede preservar la cola dañada. No se sobrescribirá; revisa la carpeta de datos."; return result; }
        }
        bytes = readDraft(m_data + "/draft.previous.json");
        result.warning = "La cola dañada se ha conservado en la carpeta de datos. ";
        if (!validDocument(bytes)) { result.warning += "No hay una copia anterior válida; las imágenes de Steam no se han tocado."; return result; }
        result.warning += "Se ha recuperado la copia anterior de la cola.";
    }
    QSet<QString> keys, files; qint64 total = 0; const QString cache = QFileInfo(m_cache).canonicalFilePath();
    for (const auto &entry : QJsonDocument::fromJson(bytes).array()) {
        const auto object = entry.toObject(), game = object.value("game").toObject();
        const double appid = game.value("appid").toDouble();
        PendingChange change; change.type = object.value("type").toString(); change.file = object.value("file").toString(); change.description = object.value("description").toString(); change.animated = object.value("animated").toBool();
        change.game.name = game.value("name").toString(); change.game.steamRoot = game.value("root").toString(); change.game.userId = game.value("user").toString(); change.game.external = game.value("external").toBool(); change.game.icon = game.value("icon").toString(); change.game.iconHash = game.value("iconHash").toString();
        const QFileInfo info(change.file); bool valid = std::isfinite(appid) && appid >= 1 && appid <= 4294967295.0 && std::floor(appid) == appid;
        if (valid) change.game.appId = static_cast<quint32>(appid);
        valid = valid && artTypes().contains(change.type) && !change.game.name.isEmpty() && change.game.name.size() <= 1000 && change.description.size() <= 1000
            && QRegularExpression("\\A[0-9]{1,20}\\z").match(change.game.userId).hasMatch()
            && ((m_demo && change.game.steamRoot == "demo") || QDir::isAbsolutePath(change.game.steamRoot))
            && !cache.isEmpty() && !Platform::isLink(info) && info.isFile() && Platform::sameOrBelow(info.canonicalFilePath(), cache)
            && info.size() > 0 && info.size() <= 64 * 1024 * 1024 && result.changes.size() < 100 && !keys.contains(change.key());
        const qint64 additional = files.contains(info.canonicalFilePath()) ? 0 : info.size();
        if (valid && total + additional > 256LL * 1024 * 1024) valid = false;
        if (valid) {
            bool animated = false; valid = Transactions::validImage(change.file, &animated);
            change.animated = change.animated || animated;
            if (change.type == "icon" && change.animated) valid = false;
        }
        const auto contentAddressed = QRegularExpression("\\Alocal-([a-f0-9]{64})\\.bin\\z").match(info.fileName());
        if (valid && contentAddressed.hasMatch()) { QFile input(change.file); valid = input.open(QIODevice::ReadOnly) && CacheStore::hash(input.readAll()) == contentAddressed.captured(1); }
        if (!valid) { ++result.rejected; continue; }
        keys.insert(change.key()); files.insert(info.canonicalFilePath()); total += additional; result.changes.append(change);
    }
    if (result.rejected) result.warning += QString(" Se omitieron %1 entradas duplicadas, incompletas o no seguras; revisa la cola antes de aplicar.").arg(result.rejected);
    return result;
}
}
