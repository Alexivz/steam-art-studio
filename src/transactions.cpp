#include "transactions.h"
#include "cache.h"
#include "steam.h"
#include "valvekv.h"
#include "imageutils.h"
#include "platform.h"
#include <QDir>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtEndian>

namespace Studio {
namespace {
QByteArray readFile(const QString &path, qint64 limit = 128 * 1024 * 1024) { QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > limit) return {}; return file.readAll(); }
QString fileHash(const QString &path) { const auto bytes = readFile(path); return bytes.isEmpty() ? QString() : CacheStore::hash(bytes); }
bool apng(const QByteArray &bytes) {
    if (!bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) return false;
    qsizetype offset = 8;
    while (offset + 12 <= bytes.size()) {
        const auto size = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
        const auto kind = bytes.mid(offset + 4, 4); if (kind == "acTL") return true; if (kind == "IDAT") return false;
        if (size > static_cast<quint32>(bytes.size() - offset - 12)) return false;
        offset += size + 12;
    }
    return false;
}
QString safeDirectory(const QString &root, const QStringList &parts) {
    QString directory = QFileInfo(root).canonicalFilePath(); if (directory.isEmpty() || !QFileInfo(directory).isDir()) return {};
    for (const auto &part : parts) {
        if (part.isEmpty() || part == "." || part == ".." || part.contains('/')) return {};
        directory += "/" + part; const QFileInfo info(directory);
        if (Platform::isLink(info) || (info.exists() && !info.isDir())) return {};
    }
    return directory;
}
QString safeConfig(const QString &root, const QString &user) {
    if (!QRegularExpression("^[0-9]+$").match(user).hasMatch()) return {};
    const auto config = safeDirectory(root, {"userdata", user, "config"});
    return QFileInfo(config).isDir() ? config : QString();
}
QString safeGrid(const QString &root, const QString &user) {
    if (safeConfig(root, user).isEmpty()) return {};
    return safeDirectory(root, {"userdata", user, "config", "grid"});
}
QString targetOf(const QJsonObject &record) {
    const QString root = record.value("root").toString(), user = record.value("user").toString();
    const QString scope = record.value("scope").toString("grid"), name = record.value("name").toString();
    QString directory;
    if (scope == "grid") {
        directory = safeGrid(root, user);
        if (!QRegularExpression("^[0-9]+(?:p|_hero|_logo|_icon)?\\.(?:png|jpg|jpeg|gif|webp|ico)$").match(name).hasMatch()
            || (name.endsWith(".ico") && !name.endsWith("_icon.ico"))) return {};
    } else if (scope == "shortcuts") {
        if (name != "shortcuts.vdf") return {};
        directory = safeConfig(root, user);
    } else if (scope == "library-icon") {
        const QString hash = record.value("hash").toString();
        bool ok = false; const quint32 id = record.value("appid").toVariant().toUInt(&ok);
        if (!ok || !id || hash.size() != 40 || !QRegularExpression("^[a-f0-9]{40}$").match(hash).hasMatch()) return {};
        const bool legacy = record.value("legacy").toBool();
        if (name != (legacy ? QString::number(id) + "_icon" : hash) + ".jpg") return {};
        directory = legacy ? safeDirectory(root, {"appcache", "librarycache"}) : safeDirectory(root, {"appcache", "librarycache", QString::number(id)});
    } else return {};
    if (directory.isEmpty()) return {};
    const QString target = directory + "/" + name; const QFileInfo info(target);
    return Platform::isLink(info) || (info.exists() && !info.isFile()) ? QString() : target;
}
bool validRecord(const QJsonObject &record) { return !targetOf(record).isEmpty(); }
QJsonObject imageRecord(const Game &game, const QString &type) {
    QJsonObject record{{"root", QFileInfo(game.steamRoot).canonicalFilePath()}, {"user", game.userId}, {"game", game.name}, {"type", type}};
    if (type == "icon" && !game.external) {
        const QString hash = game.iconHash.toLower();
        const QString base = game.steamRoot + "/appcache/librarycache/";
        const bool legacy = !QFileInfo::exists(base + QString::number(game.appId) + "/" + hash + ".jpg") && QFileInfo::exists(base + QString::number(game.appId) + "_icon.jpg");
        record["scope"] = "library-icon"; record["appid"] = static_cast<double>(game.appId); record["hash"] = hash; record["legacy"] = legacy;
        record["name"] = (legacy ? QString::number(game.appId) + "_icon" : hash) + ".jpg";
    } else { record["scope"] = "grid"; record["name"] = artBase(game.appId, type) + ".png"; }
    return record;
}
QString prepareImage(const PendingChange &change, const QString &directory, int index, AnimationProfile profile, QString *error) {
    bool animation = false;
    if (!Transactions::validImage(change.file, &animation, error)) return {};
    const auto source = readFile(change.file); const QString destination = directory + "/prepared-" + QString::number(index) + ".png";
    if (change.type == "icon") {
        if (animation || change.animated) { *error = "Los iconos deben ser estáticos. No se aplicará una animación como icono."; return {}; }
        QImageReader reader(change.file); reader.setAutoTransform(true);
        if (!selectLargestIcon(reader)) { *error = "No se ha podido leer la imagen del archivo ICO."; return {}; }
        const QSize size = reader.size(); if (size.isValid()) reader.setScaledSize(size.scaled(256, 256, Qt::KeepAspectRatio));
        const QImage image = reader.read(); if (image.isNull()) { *error = "No se ha podido decodificar el icono."; return {}; }
        QImage icon(256, 256, QImage::Format_ARGB32_Premultiplied); icon.fill(Qt::transparent);
        { QPainter painter(&icon); const QSize fitted = image.size().scaled(icon.size(), Qt::KeepAspectRatio); painter.setRenderHint(QPainter::SmoothPixmapTransform); painter.drawImage(QRect(QPoint((256 - fitted.width()) / 2, (256 - fitted.height()) / 2), fitted), image); }
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly);
        if (!icon.save(&buffer, "PNG") || !CacheStore::atomicWrite(destination, png, error)) return {};
        return destination;
    }
    if (apng(source) && profile == AnimationProfile::Original) { if (!CacheStore::atomicWrite(destination, source, error)) return {}; return destination; }
    if (animation || change.animated) {
        if (!convertAnimation(change.file, destination, change.type, profile, error)) return {};
        return destination;
    }
    QImageReader reader(change.file); reader.setAutoTransform(true);
    const auto image = reader.read();
    if (image.isNull()) { *error = "La imagen no se ha podido decodificar."; return {}; }
    QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG") || !CacheStore::atomicWrite(destination, png, error)) return {};
    return destination;
}
}
Transactions::Transactions(const QString &root, const TransactionIo &io) : m_root(QDir(root).absolutePath()), m_io(io) {
    QDir().mkpath(m_root); QFile::setPermissions(m_root, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}
bool Transactions::write(const QString &path, const QByteArray &bytes, QString *error) const {
    return m_io.write ? m_io.write(path, bytes, error) : CacheStore::atomicWrite(path, bytes, error);
}
bool Transactions::writeManifest(const QString &directory, const QJsonObject &manifest, QString *error) const {
    return write(directory + "/manifest.json", QJsonDocument(manifest).toJson(QJsonDocument::Indented), error);
}
bool Transactions::validImage(const QString &path, bool *animated, QString *error) {
    const auto info = QFileInfo(path);
    if (!info.isFile() || info.size() <= 0 || info.size() > 64 * 1024 * 1024) { if (error) *error = "La imagen no existe, está vacía o supera 64 MiB."; return false; }
    QImageReader reader(path); const auto format = reader.format().toLower();
    if (format != "png" && format != "jpg" && format != "jpeg" && format != "webp" && format != "gif" && format != "ico") {
        if (error) *error = "Para aplicar arte se admiten PNG/APNG, JPEG, WebP, GIF e ICO; no vídeos ni SVG.";
        return false;
    }
    const QSize size = reader.size();
    if (!size.isValid() || static_cast<qint64>(size.width()) * size.height() > 80LL * 1000 * 1000) { if (error) *error = "Las dimensiones de la imagen son demasiado grandes o no son válidas."; return false; }
    if (animated) *animated = apng(readFile(path)) || (format != "ico" && reader.supportsAnimation() && reader.imageCount() > 1);
    return true;
}
bool Transactions::validIcon(const QString &path, QString *error) {
    bool animated = false; if (!validImage(path, &animated, error)) return false;
    if (animated) { if (error) *error = "Los iconos deben ser estáticos; elige PNG, ICO u otra imagen sin animación."; return false; }
    return true;
}
QString Transactions::destination(const Game &game, const QString &type) {
    if (!game.appId || !artTypes().contains(type)) return {};
    return targetOf(imageRecord(game, type));
}
QString Transactions::recordDestination(const QJsonObject &record) { return targetOf(record); }
TransactionResult Transactions::apply(const QVector<PendingChange> &changes, AnimationProfile profile) const {
    TransactionResult result;
    if (changes.isEmpty() || changes.size() > 100) { result.error = "El lote debe contener entre 1 y 100 cambios."; return result; }
    QLockFile lock(m_root + "/transactions.lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock()) { result.error = "Ya hay otra operación de imágenes en curso."; return result; }
    QTemporaryDir prepared(m_root + "/.prepare-XXXXXX");
    if (!prepared.isValid()) { result.error = "No se ha podido crear el área temporal."; return result; }
    QStringList staged; qint64 total = 0; QSet<QString> destinations;
    QMap<QString, QMap<quint32, QVariantMap>> metadata;
    for (int i = 0; i < changes.size(); ++i) {
        const auto &change = changes.at(i);
        if (!change.game.appId || !artTypes().contains(change.type) || safeGrid(change.game.steamRoot, change.game.userId).isEmpty()) { result.error = "Destino de Steam no válido. No se ha escrito ningún cambio."; return result; }
        const auto target = destination(change.game, change.type);
        if (target.isEmpty()) { result.error = "No se ha podido determinar un destino seguro para la imagen o el icono."; return result; }
        if (destinations.contains(target)) { result.error = "Hay cambios duplicados para la misma imagen (los iconos nativos se comparten entre cuentas)."; return result; }
        destinations.insert(target);
        if (change.type == "icon" && !change.game.external) {
            const QString root = QFileInfo(change.game.steamRoot).canonicalFilePath();
            if (!metadata.contains(root)) {
                QString error; const auto data = ValveKv::appInfo(readFile(root + "/appcache/appinfo.vdf", 256 * 1024 * 1024), &error);
                if (!error.isEmpty()) { result.error = "No se pudo verificar el icono actual de Steam. Actualiza la biblioteca antes de aplicar."; return result; }
                metadata[root] = data;
            }
            const QString hash = ValveKv::value(ValveKv::child(metadata.value(root).value(change.game.appId), "common"), "clienticon").toString().toLower();
            if (hash.isEmpty() || hash != change.game.iconHash.toLower()) { result.error = "Cambió el identificador del icono de Steam. Actualiza la biblioteca y prepara de nuevo ese icono."; return result; }
        }
        const auto image = prepareImage(change, prepared.path(), i, profile, &result.error);
        if (image.isEmpty()) return result;
        total += QFileInfo(image).size(); if (total > 512LL * 1024 * 1024) { result.error = "El lote supera 512 MiB. Divídelo en varios lotes."; return result; }
        staged.append(image);
    }
    result.id = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-") + QUuid::createUuid().toString(QUuid::Id128).left(10);
    const auto directory = m_root + "/" + result.id; QDir().mkpath(directory);
    QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QJsonArray files; QMap<QString, QString> replacements; QMap<QString, QJsonObject> recordsByPath;
    QMap<QString, QMap<quint32, QString>> shortcutIcons; QMap<QString, Game> shortcutOwners;
    auto backupRecord = [&](QJsonObject record, const QString &after) {
        const QString target = targetOf(record);
        if (target.isEmpty() || recordsByPath.contains(target)) { result.error = "Se ha detectado una ruta no segura o duplicada."; return false; }
        const bool exists = QFileInfo::exists(target); record["existed"] = exists; record["after"] = after;
        if (exists) {
            const auto bytes = readFile(target); if (bytes.isEmpty()) { result.error = "No se ha podido respaldar un archivo existente."; return false; }
            const QString backup = "original-" + QString::number(files.size()) + ".bin";
            if (!write(directory + "/" + backup, bytes, &result.error)) return false;
            record["backup"] = backup; record["before"] = CacheStore::hash(bytes);
        }
        recordsByPath[target] = record; files.append(record); return true;
    };
    for (int i = 0; i < changes.size(); ++i) {
        const auto &change = changes.at(i); const auto target = destination(change.game, change.type);
        const auto base = artBase(change.game.appId, change.type);
        replacements[target] = staged.at(i);
        if (change.type == "icon" && !change.game.external) {
            if (!backupRecord(imageRecord(change.game, change.type), fileHash(staged.at(i)))) return result;
            continue;
        }
        QStringList extensions = {"png", "jpg", "jpeg", "gif", "webp"}; if (change.type == "icon") extensions.append("ico");
        for (const auto &extension : extensions) {
            const QString name = base + "." + extension; const QString path = safeGrid(change.game.steamRoot, change.game.userId) + "/" + name;
            const bool exists = QFileInfo::exists(path);
            if (!exists && path != target) continue;
            auto record = imageRecord(change.game, change.type); record["name"] = name;
            if (!backupRecord(record, path == target ? fileHash(staged.at(i)) : QString())) return result;
        }
        if (change.type == "icon") {
            QJsonObject record{{"scope", "shortcuts"}, {"root", change.game.steamRoot}, {"user", change.game.userId}, {"name", "shortcuts.vdf"}};
            const QString config = targetOf(record);
            if (config.isEmpty()) { result.error = "No se encuentra una configuración segura de accesos externos."; return result; }
            shortcutIcons[config][change.game.appId] = target; shortcutOwners[config] = change.game;
        }
    }
    for (auto it = shortcutIcons.cbegin(); it != shortcutIcons.cend(); ++it) {
        const auto original = readFile(it.key(), 16 * 1024 * 1024);
        const auto updated = ValveKv::shortcutIcons(original, it.value(), &result.error); if (updated.isEmpty()) return result;
        const QString stagedConfig = prepared.path() + "/shortcuts-" + QString::number(replacements.size()) + ".vdf";
        if (!CacheStore::atomicWrite(stagedConfig, updated, &result.error)) return result;
        const Game game = shortcutOwners.value(it.key());
        QJsonObject record{{"scope", "shortcuts"}, {"root", game.steamRoot}, {"user", game.userId}, {"name", "shortcuts.vdf"}, {"type", "icon"}};
        if (!backupRecord(record, CacheStore::hash(updated))) return result;
        // Reject a change between patching and backup, even if it is otherwise readable.
        if (recordsByPath.value(it.key()).value("before").toString() != CacheStore::hash(original)) { result.error = "Steam cambió los accesos externos durante la preparación. No se ha escrito en Steam."; return result; }
        replacements[it.key()] = stagedConfig;
    }
    QJsonObject manifest{{"version", 2}, {"id", result.id}, {"created", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"status", "applying"}, {"changes", changes.size()}, {"files", files}};
    if (!writeManifest(directory, manifest, &result.error)) return result;
    // Confirm the files still match the exact originals we backed up.
    for (const auto &item : files) {
        const auto record = item.toObject(); const auto target = targetOf(record);
        if (target.isEmpty() || (record.value("existed").toBool() ? fileHash(target) != record.value("before").toString() : QFileInfo::exists(target))) {
            result.error = "Steam u otro programa cambió una imagen durante la preparación. Se ha cancelado sin sobrescribirla.";
            manifest["status"] = "cancelled"; writeManifest(directory, manifest); return result;
        }
    }
    bool failed = false;
    for (auto it = replacements.cbegin(); it != replacements.cend(); ++it) {
        const auto record = recordsByPath.value(it.key());
        if (targetOf(record) != it.key()) { result.error = "Cambió la ruta de destino durante la operación."; failed = true; break; }
        if (record.value("existed").toBool() ? fileHash(it.key()) != record.value("before").toString() : QFileInfo::exists(it.key())) { result.error = "Otro programa cambió el archivo antes de escribir. Se detuvo el lote para no sobrescribirlo."; failed = true; break; }
        if (!QDir().mkpath(QFileInfo(it.key()).absolutePath()) || !write(it.key(), readFile(it.value()), &result.error)) { failed = true; break; }
        if (m_io.afterMutation) m_io.afterMutation(it.key());
    }
    if (!failed) for (const auto &item : files) {
        const auto record = item.toObject(); const auto target = targetOf(record);
        if (target.isEmpty()) { result.error = "La ruta de destino ha dejado de ser segura."; failed = true; break; }
        if (record.value("after").toString().isEmpty() && QFileInfo::exists(target)) {
            if (fileHash(target) != record.value("before").toString() || !QFile::remove(target)) { result.error = "No se pudo retirar una versión anterior o cambió durante el lote."; failed = true; break; }
            if (m_io.afterMutation) m_io.afterMutation(target);
        }
    }
    if (failed) {
        const auto rollback = restoreImpl(result.id, manifest);
        if (!rollback.ok) { manifest["status"] = "needs-attention"; writeManifest(directory, manifest); result.error += " La restauración necesita revisión en Copias."; }
        return result;
    }
    manifest["status"] = "committed";
    if (!writeManifest(directory, manifest, &result.error)) { result.error = "Los cambios se escribieron, pero el registro final no pudo actualizarse. Conserva la copia y revisa Copias."; return result; }
    result.ok = true; result.files = changes.size(); return result;
}
TransactionResult Transactions::restore(const QString &id) const {
    TransactionResult result; result.id = id;
    if (!QRegularExpression("^[0-9]{8}-[0-9]{6}-[a-f0-9]{10}$").match(id).hasMatch()) { result.error = "Identificador de copia no válido."; return result; }
    QLockFile lock(m_root + "/transactions.lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock()) { result.error = "Ya hay otra operación en curso."; return result; }
    const auto path = m_root + "/" + id;
    if (QFileInfo(path).isSymLink()) { result.error = "Ruta de copia no segura."; return result; }
    const auto manifest = QJsonDocument::fromJson(readFile(path + "/manifest.json")).object();
    return restoreImpl(id, manifest);
}
TransactionResult Transactions::restoreImpl(const QString &id, const QJsonObject &input) const {
    TransactionResult result; result.id = id; auto manifest = input;
    const auto status = manifest.value("status").toString(); const auto files = manifest.value("files").toArray();
    const int version = manifest.value("version").toInt();
    if ((version != 1 && version != 2) || manifest.value("id").toString() != id || files.isEmpty() || (status != "committed" && status != "applying" && status != "restoring" && status != "needs-attention")) { result.error = "Esta copia no se puede restaurar o ya fue restaurada."; return result; }
    const auto directory = m_root + "/" + id;
    for (const auto &item : files) {
        const auto record = item.toObject(); if (!validRecord(record)) { result.error = "La copia contiene una ruta no válida."; return result; }
        const QString backup = record.value("backup").toString();
        if (record.value("existed").toBool() && (!QRegularExpression("^original-[0-9]+\\.bin$").match(backup).hasMatch()
            || fileHash(directory + "/" + backup) != record.value("before").toString())) { result.error = "La copia está incompleta o cambió su contenido."; return result; }
        const auto current = fileHash(targetOf(record)); const bool exists = QFileInfo::exists(targetOf(record));
        const auto before = record.value("before").toString(); const auto after = record.value("after").toString();
        if (exists && (current.isEmpty() || (current != before && current != after))) { result.error = "Una imagen cambió después de esta operación. No se sobrescribirá: conserva la copia y revisa el conflicto."; return result; }
        if (!exists && !after.isEmpty() && status == "committed") { result.error = "Una imagen fue retirada por otro programa. No se restaurará sin revisar el conflicto."; return result; }
    }
    manifest["status"] = "restoring";
    if (!writeManifest(directory, manifest, &result.error)) return result;
    for (const auto &item : files) {
        const auto record = item.toObject(); const auto target = targetOf(record);
        if (target.isEmpty()) { result.error = "La ruta ha dejado de ser segura; conserva la copia y revisa el destino."; return result; }
        const QString current = fileHash(target);
        if (QFileInfo::exists(target) && (current.isEmpty() || (current != record.value("before").toString() && current != record.value("after").toString()))) { result.error = "El archivo cambió durante la restauración. No se sobrescribirá; conserva la copia."; return result; }
        if (record.value("existed").toBool()) {
            if (current == record.value("before").toString()) continue;
            const auto original = readFile(directory + "/" + record.value("backup").toString());
            if (original.isEmpty() || CacheStore::hash(original) != record.value("before").toString()) { result.error = "La copia cambió durante la restauración; no se escribirá un original incompleto."; return result; }
            if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !write(target, original, &result.error)) return result;
            if (m_io.afterMutation) m_io.afterMutation(target);
        } else if (QFileInfo::exists(target)) {
            if (!QFile::remove(target)) { result.error = "No se pudo retirar una imagen creada por la operación."; return result; }
            if (m_io.afterMutation) m_io.afterMutation(target);
        }
    }
    manifest["status"] = "restored"; manifest["restored"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!writeManifest(directory, manifest, &result.error)) return result;
    result.ok = true; result.files = manifest.value("changes").toInt(); return result;
}
QJsonArray Transactions::history() const {
    QJsonArray result;
    for (const auto &directory : QDir(m_root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name | QDir::Reversed)) {
        const auto manifest = QJsonDocument::fromJson(readFile(directory.absoluteFilePath() + "/manifest.json")).object();
        if (manifest.value("version").toInt() == 1 || manifest.value("version").toInt() == 2) result.append(manifest);
    }
    return result;
}
}
