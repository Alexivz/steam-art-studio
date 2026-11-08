#pragma once
#include "types.h"
#include <QSet>
#include <array>

namespace Studio {
struct LibraryFilter {
    QString text;
    int status = 0, order = 0;
    bool favoritesOnly = false, collectionOnly = false;
    QSet<QString> favorites, collection;
};
class LibraryIndex {
public:
    void setGames(const QVector<Game> &games);
    QVector<Game> query(const LibraryFilter &filter, qint64 now = QDateTime::currentSecsSinceEpoch()) const;
    static QString normalize(const QString &text);
private:
    QVector<Game> m_games;
    QVector<QString> m_names, m_keys;
    std::array<QVector<int>, 4> m_order;
};
}
