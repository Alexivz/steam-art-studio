#pragma once
#include <QColor>
#include <QIcon>
#include <QListView>
#include <QWidget>

namespace Studio {
class ThumbnailLoader;
namespace Ui {
enum class Symbol { Library, Star, Collection, Queue, Backup, Settings, Search, Refresh, Plus, Image, Preview, Left, Right, Panel, Check, Cover, Wide, Hero, Logo, GameIcon, Close, Trash, Link, Play, Pause, Warning, Save, Download };
QIcon icon(Symbol symbol, const QColor &color = QColor("#a2b5c8"));
Symbol artSymbol(const QString &type);
bool confirmSteamChange(QWidget *parent, const QString &title, const QString &text, bool icons, const QString &details = {});
QString stylesheet();
}
class CardView : public QListView {
public:
    explicit CardView(QWidget *parent = nullptr);
    void setEmptyState(const QString &title, const QString &description);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QString m_emptyTitle, m_emptyDescription;
};
class ArtworkTile : public QWidget {
public:
    explicit ArtworkTile(ThumbnailLoader *loader, QWidget *parent = nullptr);
    void setFile(const QString &file);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    ThumbnailLoader *m_loader;
    QString m_file;
};
}
