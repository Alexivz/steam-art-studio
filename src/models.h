#pragma once
#include "types.h"
#include <QAbstractListModel>
#include <QCache>
#include <QSet>
#include <QStyledItemDelegate>
#include <QThreadPool>

namespace Studio {
enum ItemRole { ObjectRole = Qt::UserRole + 1, ImageRole, SubtitleRole, AnimatedRole, FavoriteRole, InstalledRole };
class ThumbnailLoader : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailLoader(const QString &posterDirectory, QObject *parent = nullptr);
    ~ThumbnailLoader() override;
    QImage image(const QString &file, const QSize &size);
signals:
    void loaded();
private:
    QString m_posters;
    QCache<QString, QImage> m_images;
    QSet<QString> m_pending;
    QThreadPool m_pool;
};
class CardModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit CardModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    void setGames(const QVector<Game> &games, const QSet<QString> &favorites);
    void setArtworks(const QVector<Artwork> &artworks);
    void setThumbnail(int row, const QString &file);
    Game game(int row) const;
    Artwork artwork(int row) const;
    bool artworkMode() const { return m_artMode; }
    quint64 revision() const { return m_revision; }
signals:
    void thumbnailNeeded(int row, const QUrl &url, quint64 revision) const;
private:
    QVector<Game> m_games;
    QVector<Artwork> m_art;
    QVector<QString> m_files;
    QSet<QString> m_favorites;
    mutable QSet<int> m_requested;
    bool m_artMode = false;
    quint64 m_revision = 0;
};
class CardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit CardDelegate(ThumbnailLoader *loader, bool small = false, QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
    void setWide(bool wide) { m_wide = wide; }
    void setCellSize(const QSize &size) { m_cellSize = size; }
    void setContain(bool contain) { m_contain = contain; }
private:
    ThumbnailLoader *m_loader;
    bool m_small, m_wide = false, m_contain = false;
    QSize m_cellSize;
};
QImage demoCover(const QString &title, int seed, const QSize &size = QSize(480, 720));
QImage demoIcon(const QString &title, int seed);
}
