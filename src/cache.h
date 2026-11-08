#pragma once
#include <QDateTime>
#include <QSet>
#include <QHash>
#include <QString>
#include <QUrl>

namespace Studio {
struct CacheEntry {
    QString file;
    QString mime;
    QDateTime stored;
    bool fresh = false;
    bool valid() const { return !file.isEmpty(); }
};
class CacheStore {
public:
    explicit CacheStore(const QString &root, qint64 budget = 512LL * 1024 * 1024);
    QString root() const { return m_root; }
    CacheEntry lookup(const QUrl &url, qint64 freshSeconds, qint64 maximumSeconds = 86400) const;
    QString store(const QUrl &url, const QByteArray &data, const QString &mime, QString *error = nullptr);
    QString importFile(const QString &source, QString *error = nullptr);
    void pin(const QString &file);
    void unpin(const QString &file);
    qint64 bytes() const;
    qint64 budget() const { return m_budget; }
    void prune();
    void clearUnpinned();
    static QString hash(const QByteArray &data);
    static bool atomicWrite(const QString &path, const QByteArray &data, QString *error = nullptr);
private:
    QString m_root;
    qint64 m_budget;
    QHash<QString, int> m_pinned;
};
}
