#pragma once
#include "cache.h"
#include "types.h"
#include <QHash>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QSharedPointer>

namespace Studio {
struct NetworkPolicy {
    int connectionMs = 5000, transferMs = 15000, deadlineMs = 30000, retryMs = 600;
    // Originals can be much larger than API responses/thumbnails. All limits
    // remain bounded; receiving bytes resets inactivity, never the deadline.
    int originalConnectionMs = 15000, originalTransferMs = 30000, originalDeadlineMs = 180000;
    // API requests may reuse TLS, so encrypted() is not a connection milestone.
    // Bound first response and idle separately; total includes retry/redirects.
    int apiResponseMs = 30000, apiTransferMs = 30000, apiDeadlineMs = 90000;
};
class Network : public QObject {
    Q_OBJECT
public:
    explicit Network(CacheStore *cache, QObject *parent = nullptr, QNetworkAccessManager *transport = nullptr, const NetworkPolicy &policy = {});
    ~Network() override;
    quint64 fetch(const QUrl &url, const QString &kind = "image");
    void cancel(quint64 token);
    void cancelAll();
    void setApiKey(const QString &key);
    bool hasApiKey() const { return !m_key.isEmpty(); }
    static bool allowed(const QUrl &url, bool api);
signals:
    void ready(quint64 token, const QString &file, bool fromCache);
    void failed(quint64 token, const QString &message, int status = 0);
    void activity(int active, int queued);
    void progress(quint64 token, qint64 received, qint64 total);
    void cancelled(quint64 token);
private:
    struct Job {
        QUrl original, current;
        QString kind, key, timeoutMessage;
        QVector<quint64> tokens;
        QByteArray data;
        QElapsedTimer elapsed;
        QNetworkReply *reply = nullptr;
        int attempts = 0, redirects = 0;
        bool oversized = false, active = false, timedOut = false, cancelled = false, deadlineExpired = false;
    };
    CacheStore *m_cache;
    QNetworkAccessManager m_manager;
    QNetworkAccessManager *m_transport;
    NetworkPolicy m_policy;
    QSet<quint64> m_live;
    QHash<quint64, QString> m_cachedPins;
    QHash<QString, qint64> m_backoffHosts;
    QString m_key;
    quint64 m_next = 0;
    int m_active = 0, m_images = 0;
    bool m_cancelling = false;
    QHash<QString, QSharedPointer<Job>> m_jobs;
    QList<QSharedPointer<Job>> m_queue;
    void pump();
    void start(const QSharedPointer<Job> &job);
    void finish(const QSharedPointer<Job> &job);
    void deliverReady(quint64 token, const QString &file, bool cached);
    void deliverFailure(quint64 token, const QString &message, int status = 0);
};

class ArtService : public QObject {
    Q_OBJECT
public:
    explicit ArtService(Network *network, QObject *parent = nullptr);
    void search(const Game &game, const QString &type, bool statics, bool animations, bool adult, int page);
    void invalidate();
    void setMapping(const QString &gameKey, qint64 sgdbId);
    static QUrl artworkUrl(const QString &type, qint64 id, bool statics, bool animations, bool adult, int page);
signals:
    void results(const QVector<Studio::Artwork> &artworks, int page, int total, bool cached);
    void error(const QString &message);
    void loading(const QString &message);
private:
    Network *m_network;
    Game m_game;
    QString m_type;
    bool m_static = true, m_animated = true, m_adult = false;
    int m_page = 0;
    quint64 m_generation = 0;
    struct Context { quint64 generation; QString action; };
    QHash<quint64, Context> m_requests;
    QHash<QString, qint64> m_gameIds;
    void request(const QUrl &url, const QString &action);
    void requestArt(qint64 id);
    void received(quint64 token, const QString &file, bool cached);
};
}
