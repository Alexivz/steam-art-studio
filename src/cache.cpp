#include "cache.h"
#include "platform.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

namespace Studio {
CacheStore::CacheStore(const QString &root, qint64 budget) : m_root(QDir(root).absolutePath()), m_budget(budget) {
    for (const auto &folder : {m_root, m_root + "/data", m_root + "/metadata", m_root + "/posters"}) {
        if (Platform::isLink(QFileInfo(folder))) continue;
        QDir().mkpath(folder); QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
}
QString CacheStore::hash(const QByteArray &data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()); }
bool CacheStore::atomicWrite(const QString &path, const QByteArray &data, QString *error) {
    if (Platform::isLink(QFileInfo(path))) { if (error) *error = "No se escribirá a través de un enlace simbólico o punto de reanálisis."; return false; }
    QSaveFile file(path); file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(data) != data.size() || !file.commit()) { if (error) *error = file.errorString(); return false; }
    return true;
}
CacheEntry CacheStore::lookup(const QUrl &url, qint64 freshSeconds, qint64 maximumSeconds) const {
    if (Platform::isLink(QFileInfo(m_root + "/data")) || Platform::isLink(QFileInfo(m_root + "/metadata"))) return {};
    const auto key = hash(url.toEncoded(QUrl::FullyEncoded)); QFile file(m_root + "/metadata/" + key + ".json");
    if (Platform::isLink(QFileInfo(file)) || !file.open(QIODevice::ReadOnly) || file.size() > 16384) return {};
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    const auto stored = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(object.value("stored").toDouble()));
    const auto age = stored.secsTo(QDateTime::currentDateTimeUtc());
    const QString data = m_root + "/data/" + key + ".bin";
    if (object.value("version").toInt() != 1 || object.value("url").toString() != url.toString(QUrl::FullyEncoded)
        || !stored.isValid() || age < -300 || age > maximumSeconds || !QFileInfo(data).isFile() || Platform::isLink(QFileInfo(data)) || QFileInfo(data).size() < 1) return {};
    QFile touch(data); if (touch.open(QIODevice::ReadOnly)) touch.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
    return {data, object.value("mime").toString(), stored, age <= freshSeconds};
}
QString CacheStore::store(const QUrl &url, const QByteArray &data, const QString &mime, QString *error) {
    if (Platform::isLink(QFileInfo(m_root + "/data")) || Platform::isLink(QFileInfo(m_root + "/metadata"))) { if (error) *error = "La carpeta de caché no es segura."; return {}; }
    if (data.isEmpty() || data.size() > 64 * 1024 * 1024) { if (error) *error = "El archivo está vacío o supera el límite de 64 MiB."; return {}; }
    const auto key = hash(url.toEncoded(QUrl::FullyEncoded)); const auto path = m_root + "/data/" + key + ".bin";
    // A refreshed URL must never replace a queued/previewed original in place.
    if (m_pinned.contains(Platform::pathKey(path)) && QFileInfo(path).isFile()) return path;
    if (!atomicWrite(path, data, error)) return {};
    const QJsonObject metadata{{"version", 1}, {"url", url.toString(QUrl::FullyEncoded)}, {"mime", mime}, {"stored", static_cast<double>(QDateTime::currentMSecsSinceEpoch())}};
    if (!atomicWrite(m_root + "/metadata/" + key + ".json", QJsonDocument(metadata).toJson(QJsonDocument::Compact), error)) { QFile::remove(path); return {}; }
    // Protect the new entry until consumers receive it and establish their pins.
    pin(path); prune(); unpin(path);
    return path;
}
QString CacheStore::importFile(const QString &source, QString *error) {
    if (Platform::isLink(QFileInfo(m_root + "/data"))) { if (error) *error = "La carpeta de originales no es segura."; return {}; }
    QFile file(source);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > 64 * 1024 * 1024) { if (error) *error = "No se puede leer la imagen o supera 64 MiB."; return {}; }
    const auto data = file.readAll(); const auto key = hash(data); const QString destination = m_root + "/data/local-" + key + ".bin";
    if (Platform::isLink(QFileInfo(destination))) { if (error) *error = "No se usará un enlace como original de la cola."; return {}; }
    if (QFileInfo::exists(destination)) { QFile existing(destination); if (!existing.open(QIODevice::ReadOnly) || existing.size() > 64 * 1024 * 1024 || hash(existing.readAll()) != key) { if (error) *error = "Un original fijado de la caché cambió. No se sustituirá mientras pueda estar en la cola."; return {}; } }
    if (!QFileInfo::exists(destination) && !atomicWrite(destination, data, error)) return {};
    pin(destination); prune(); unpin(destination);
    return destination;
}
void CacheStore::pin(const QString &file) { ++m_pinned[Platform::pathKey(file)]; }
void CacheStore::unpin(const QString &file) {
    auto it = m_pinned.find(Platform::pathKey(file));
    if (it != m_pinned.end() && --it.value() <= 0) m_pinned.erase(it);
}
qint64 CacheStore::bytes() const {
    qint64 total = 0; QDirIterator it(m_root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); total += it.fileInfo().size(); } return total;
}
void CacheStore::prune() {
    if (Platform::isLink(QFileInfo(m_root + "/data")) || Platform::isLink(QFileInfo(m_root + "/metadata")) || Platform::isLink(QFileInfo(m_root + "/posters"))) return;
    qint64 total = bytes(); if (total <= m_budget) return;
    auto files = QDir(m_root + "/data").entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Time | QDir::Reversed);
    for (const auto &file : files) {
        if (total <= m_budget * 9 / 10) break;
        if (Platform::isLink(file) || m_pinned.contains(Platform::pathKey(file.absoluteFilePath()))) continue;
        if (QFile::remove(file.absoluteFilePath())) {
            total -= file.size(); const auto key = file.completeBaseName(); QFile::remove(m_root + "/metadata/" + key + ".json");
        }
    }
    auto posters = QDir(m_root + "/posters").entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Time | QDir::Reversed);
    for (const auto &file : posters) if (total > m_budget && !Platform::isLink(file) && !m_pinned.contains(Platform::pathKey(file.absoluteFilePath())) && QFile::remove(file.absoluteFilePath())) total -= file.size();
}
void CacheStore::clearUnpinned() {
    // Only managed image folders: never marker, lock, drafts, backups or unknown
    // files placed at the cache root. No recursive deletion.
    for (const QString &folder : {m_root + "/data", m_root + "/posters"})
        for (const auto &file : QDir(folder).entryInfoList(QDir::Files | QDir::NoSymLinks))
            if (!Platform::isLink(QFileInfo(folder)) && !Platform::isLink(file))
            if (!m_pinned.contains(Platform::pathKey(file.absoluteFilePath()))) QFile::remove(file.absoluteFilePath());
    for (const auto &file : QDir(m_root + "/metadata").entryInfoList({"*.json"}, QDir::Files | QDir::NoSymLinks)) {
        if (!Platform::isLink(QFileInfo(m_root + "/metadata")) && !Platform::isLink(file) && !QFileInfo::exists(m_root + "/data/" + file.completeBaseName() + ".bin")) QFile::remove(file.absoluteFilePath());
    }
}
}
