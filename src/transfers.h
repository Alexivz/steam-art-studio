#pragma once
#include "network.h"
#include <QAbstractListModel>

namespace Studio {
struct ArtTask { Game game; Artwork art; QString purpose; };
class TransferModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum State { Waiting, Downloading, Complete, Failed, Cancelled };
    enum Role { StateRole = Qt::UserRole + 1, ReceivedRole, TotalRole, ErrorRole };
    explicit TransferModel(Network *network, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool enqueue(const QUrl &url, const QString &title, const ArtTask &task);
    void cancel(int row);
    void retry(int row);
    void clearFinished();
    int activeCount() const;
signals:
    void completed(const Studio::ArtTask &task, const QString &file, bool cached);
    void changed();
    void error(const QString &message);
private:
    struct Entry { QUrl url; QString title, error; ArtTask task; quint64 token = 0; qint64 received = 0, total = -1; State state = Waiting; };
    Network *m_network;
    QVector<Entry> m_entries;
    int find(quint64 token) const;
    void notify(int row);
};
}
Q_DECLARE_METATYPE(Studio::ArtTask)
