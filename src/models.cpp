#include "models.h"
#include "cache.h"
#include "ui.h"
#include "imageutils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFutureWatcher>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QTextLayout>
#include <QtConcurrent>

namespace Studio {
ThumbnailLoader::ThumbnailLoader(const QString &posters, QObject *parent) : QObject(parent), m_posters(posters), m_images(64 * 1024) {
    m_pool.setMaxThreadCount(2); QDir().mkpath(posters);
}
ThumbnailLoader::~ThumbnailLoader() { m_pool.waitForDone(); }
QImage ThumbnailLoader::image(const QString &file, const QSize &size) {
    if (file.isEmpty()) return {};
    const auto key = file + ":" + QString::number(QFileInfo(file).lastModified().toMSecsSinceEpoch()) + ":" + QString::number(size.width()) + "x" + QString::number(size.height());
    if (auto cached = m_images.object(key)) return *cached;
    if (m_pending.contains(key) || m_pending.size() >= 48) return {};
    m_pending.insert(key); auto watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, key] {
        auto result = watcher->result(); const int cost = qMax(1, static_cast<int>(result.sizeInBytes() / 1024));
        m_images.insert(key, new QImage(result), cost); m_pending.remove(key); watcher->deleteLater(); emit loaded();
    });
    watcher->setFuture(QtConcurrent::run(&m_pool, [file, size, directory = m_posters, key] {
        const auto info = QFileInfo(file); if (!info.isFile() || info.size() > 64 * 1024 * 1024) return QImage();
        QImageReader reader(file); reader.setAutoTransform(true);
        if (!selectLargestIcon(reader)) return QImage();
        const auto dimensions = reader.size();
        if (reader.canRead() && dimensions.isValid() && static_cast<qint64>(dimensions.width()) * dimensions.height() <= 80LL * 1000 * 1000) {
            reader.setScaledSize(dimensions.scaled(size, Qt::KeepAspectRatioByExpanding)); return reader.read();
        }
        QFile header(file); if (!header.open(QIODevice::ReadOnly)) return QImage(); const auto signature = header.read(16);
        if (!signature.startsWith(QByteArray::fromHex("1a45dfa3")) && signature.mid(4, 4) != "ftyp") return QImage();
        const auto poster = directory + "/" + CacheStore::hash(key.toUtf8()) + ".png";
        if (QFileInfo::exists(poster)) return QImage(poster);
        const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg"); if (ffmpeg.isEmpty()) return QImage();
        QProcess process; process.start(ffmpeg, {"-v", "error", "-nostdin", "-protocol_whitelist", "file,pipe", "-i", file,
            "-frames:v", "1", "-vf", QString("scale=%1:%2:force_original_aspect_ratio=decrease").arg(size.width()).arg(size.height()), "-y", poster});
        if (!process.waitForFinished(5000)) { process.kill(); process.waitForFinished(1000); return QImage(); }
        if (process.exitCode() != 0) return QImage();
        return QImage(poster);
    }));
    return {};
}
CardModel::CardModel(QObject *parent) : QAbstractListModel(parent) {}
int CardModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_artMode ? m_art.size() : m_games.size(); }
QVariant CardModel::data(const QModelIndex &index, int role) const {
    const int row = index.row(); if (!index.isValid() || row < 0 || row >= rowCount()) return {};
    if (m_artMode) {
        const auto &art = m_art.at(row);
        if (role == Qt::DisplayRole) return art.author.isEmpty() ? QStringLiteral("Diseño de la comunidad") : art.author;
        if (role == SubtitleRole) return art.width <= 0 || art.height <= 0 ? QStringLiteral("ICO · Varias resoluciones") : QString("%1 × %2%3").arg(art.width).arg(art.height).arg(art.animated ? " · animada" : "");
        if (role == Qt::ToolTipRole) return art.author + "\n" + data(index, SubtitleRole).toString();
        if (role == AnimatedRole) return art.animated;
        if (role == ObjectRole) return QVariant::fromValue(art);
        if (role == ImageRole) {
            if (m_files.at(row).isEmpty() && !m_requested.contains(row)) {
                m_requested.insert(row); emit thumbnailNeeded(row, art.thumbnail, m_revision);
            }
            return m_files.at(row);
        }
    } else {
        const auto &game = m_games.at(row);
        if (role == Qt::DisplayRole) return game.name;
        if (role == Qt::ToolTipRole) return game.name + (game.external ? "\nAcceso externo de Steam" : game.cachedOnly ? "\nDatos de la caché local; no es una comprobación de licencia" : "");
        if (role == ImageRole) return game.cover;
        if (role == ObjectRole) return QVariant::fromValue(game);
        if (role == FavoriteRole) return m_favorites.contains(game.key());
        if (role == InstalledRole) return game.installed;
        if (role == SubtitleRole) return game.external ? "Externo" : game.installed ? "Instalado" : game.cachedOnly ? "En caché local" : "Biblioteca local";
    }
    return {};
}
void CardModel::setGames(const QVector<Game> &games, const QSet<QString> &favorites) { beginResetModel(); ++m_revision; m_games = games; m_art.clear(); m_files.clear(); m_requested.clear(); m_favorites = favorites; m_artMode = false; endResetModel(); }
void CardModel::setArtworks(const QVector<Artwork> &artworks) { beginResetModel(); ++m_revision; m_art = artworks; m_games.clear(); m_files = QVector<QString>(artworks.size()); m_requested.clear(); m_artMode = true; endResetModel(); }
void CardModel::setThumbnail(int row, const QString &file) { if (row >= 0 && row < m_files.size()) { m_files[row] = file; emit dataChanged(index(row), index(row), {ImageRole}); } }
Game CardModel::game(int row) const { return !m_artMode && row >= 0 && row < m_games.size() ? m_games.at(row) : Game(); }
Artwork CardModel::artwork(int row) const { return m_artMode && row >= 0 && row < m_art.size() ? m_art.at(row) : Artwork(); }
CardDelegate::CardDelegate(ThumbnailLoader *loader, bool small, QObject *parent) : QStyledItemDelegate(parent), m_loader(loader), m_small(small) {}
QSize CardDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return m_cellSize.isValid() ? m_cellSize : m_wide ? QSize(200, 174) : QSize(m_small ? 148 : 164, m_small ? 250 : 278); }
void CardDelegate::paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &index) const {
    p->save(); p->setRenderHint(QPainter::Antialiasing); p->setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF card = QRectF(o.rect).adjusted(6, 5, -6, -5); const bool selected = o.state.testFlag(QStyle::State_Selected); const bool hover = o.state.testFlag(QStyle::State_MouseOver);
    p->setPen(QPen(selected ? QColor("#50dec5") : hover ? QColor("#465063") : QColor("#282e3b"), selected ? 2 : 1));
    p->setBrush(selected ? QColor("#1e3236") : hover ? QColor("#222936") : QColor("#191e28")); p->drawRoundedRect(card, 12, 12);
    const QRectF art = card.adjusted(7, 7, -7, -68); QPainterPath clip; clip.addRoundedRect(art, 8, 8);
    p->save(); p->setClipPath(clip);
    QLinearGradient gradient(art.topLeft(), art.bottomRight()); const int hue = (index.row() * 37 + 186) % 360;
    gradient.setColorAt(0, QColor::fromHsv(hue, 115, 73)); gradient.setColorAt(1, QColor("#151824")); p->fillRect(art, gradient);
    const auto image = m_loader->image(index.data(ImageRole).toString(), art.size().toSize() * 2);
    if (!image.isNull()) {
        const auto scaled = image.size().scaled(art.size().toSize(), m_contain ? Qt::KeepAspectRatio : Qt::KeepAspectRatioByExpanding);
        const QRectF destination(art.center().x() - scaled.width() / 2., art.center().y() - scaled.height() / 2., scaled.width(), scaled.height()); p->drawImage(destination, image);
    } else {
        static const QIcon placeholder = Ui::icon(Ui::Symbol::Image, QColor("#6c8c9d"));
        placeholder.paint(p, QRect(art.center().toPoint() - QPoint(20, 20), QSize(40, 40)));
    }
    p->restore();
    if (index.data(FavoriteRole).toBool()) { p->setPen(QColor("#ffe09a")); p->drawText(art.adjusted(7, 3, -7, -3), Qt::AlignTop | Qt::AlignRight, QStringLiteral("★")); }
    if (index.data(AnimatedRole).toBool()) {
        const QRectF badge(art.left() + 6, art.top() + 6, 48, 21); p->setPen(Qt::NoPen); p->setBrush(QColor(11, 17, 24, 210)); p->drawRoundedRect(badge, 6, 6);
        QFont badgeFont = o.font; badgeFont.setPixelSize(9); badgeFont.setBold(true); p->setFont(badgeFont); p->setPen(QColor("#7ee4cf")); p->drawText(badge, Qt::AlignCenter, "ANIM");
    }
    QFont title = o.font; title.setPixelSize(12); title.setBold(true); p->setFont(title); p->setPen(QColor("#e3e8f1"));
    const QRectF text(card.left() + 10, card.bottom() - 59, card.width() - 20, 34);
    const QString name = index.data().toString().simplified();
    QTextLayout titleLayout(name, title); QTextOption wrap; wrap.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); titleLayout.setTextOption(wrap);
    titleLayout.beginLayout(); auto firstLine = titleLayout.createLine();
    if (firstLine.isValid()) firstLine.setLineWidth(text.width());
    titleLayout.endLayout();
    if (firstLine.isValid()) {
        firstLine.draw(p, text.topLeft());
        const QString rest = name.mid(firstLine.textLength()).trimmed();
        if (!rest.isEmpty()) p->drawText(QPointF(text.left(), text.top() + firstLine.height() + QFontMetrics(title).ascent()), QFontMetrics(title).elidedText(rest, Qt::ElideRight, text.width()));
    }
    QFont small = o.font; small.setPixelSize(11); small.setBold(false); p->setFont(small); p->setPen(QColor("#a0b0c5"));
    p->drawText(QRectF(card.left() + 10, card.bottom() - 20, card.width() - 20, 14), Qt::AlignVCenter, p->fontMetrics().elidedText(index.data(SubtitleRole).toString(), Qt::ElideRight, static_cast<int>(card.width() - 20)));
    p->restore();
}
QImage demoIcon(const QString &title, int seed) {
    QImage image(256, 256, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient gradient(0, 0, 256, 256); gradient.setColorAt(0, QColor::fromHsv((seed * 43 + 170) % 360, 150, 205)); gradient.setColorAt(1, QColor("#142239"));
    p.setPen(QPen(QColor(200, 255, 235, 160), 3)); p.setBrush(gradient); p.drawRoundedRect(QRectF(14, 14, 228, 228), 58, 58);
    p.setPen(QPen(QColor(255, 255, 255, 45), 2)); p.drawEllipse(QRectF(42, 42, 172, 172));
    QFont font("Noto Sans"); font.setPixelSize(110); font.setBold(true); p.setFont(font); p.setPen(QColor("#e4fff7"));
    p.drawText(image.rect(), Qt::AlignCenter, title.left(1).toUpper()); return image;
}
QImage demoCover(const QString &title, int seed, const QSize &size) {
    QImage image(size, QImage::Format_ARGB32_Premultiplied); QPainter p(&image); p.setRenderHint(QPainter::Antialiasing);
    const QColor first = QColor::fromHsv((seed * 43 + 170) % 360, 170, 170), second = QColor::fromHsv((seed * 43 + 230) % 360, 180, 45);
    QLinearGradient gradient(0, 0, size.width(), size.height()); gradient.setColorAt(0, first); gradient.setColorAt(1, second); p.fillRect(image.rect(), gradient);
    p.setPen(QPen(QColor(255, 255, 255, 38), 2));
    for (int i = 0; i < 10; ++i) p.drawEllipse(QPointF(size.width() * .75, size.height() * .45), size.width() * (.18 + i * .06), size.width() * (.18 + i * .06));
    QPolygonF mountain; mountain << QPointF(0, size.height() * .83) << QPointF(size.width() * .37, size.height() * .37) << QPointF(size.width() * .57, size.height() * .67) << QPointF(size.width() * .78, size.height() * .43) << QPointF(size.width(), size.height() * .72) << QPointF(size.width(), size.height()) << QPointF(0, size.height());
    p.setPen(Qt::NoPen); p.setBrush(QColor(8, 16, 29, 190)); p.drawPolygon(mountain);
    p.setPen(QColor(255, 255, 255, 155)); QFont small("Noto Sans"); small.setPixelSize(size.width() / 33); small.setLetterSpacing(QFont::AbsoluteSpacing, 4); p.setFont(small);
    p.drawText(QRect(30, 35, size.width() - 60, 40), Qt::AlignCenter, "ART STUDIO / ORIGINAL");
    QFont font("Noto Sans"); font.setPixelSize(size.width() / 10); font.setBold(true); font.setLetterSpacing(QFont::AbsoluteSpacing, 2); p.setFont(font); p.setPen(Qt::white);
    p.drawText(QRect(size.width() / 12, size.height() * 63 / 100, size.width() * 10 / 12, size.height() * 25 / 100), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, title.toUpper());
    p.setPen(QColor("#6df1cf")); p.drawLine(size.width() / 12, size.height() * 91 / 100, size.width() * 5 / 12, size.height() * 91 / 100); return image;
}
}
