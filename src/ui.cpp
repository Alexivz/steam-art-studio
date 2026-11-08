#include "ui.h"
#include "models.h"
#include <QPainter>
#include <QPainterPath>
#include <QMessageBox>
#include <QCheckBox>
#include <QAbstractButton>
#include <cmath>

namespace Studio {
QIcon Ui::icon(Symbol symbol, const QColor &color) {
    QPixmap pixmap(48, 48); pixmap.fill(Qt::transparent); pixmap.setDevicePixelRatio(2);
    QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    switch (symbol) {
    case Symbol::Library:
        for (int x : {4, 13}) for (int y : {4, 13}) p.drawRoundedRect(QRectF(x, y, 7, 7), 1.4, 1.4);
        break;
    case Symbol::Star: {
        QPolygonF star; for (int i = 0; i < 10; ++i) {
            const double angle = i * 3.14159265 / 5 - 3.14159265 / 2;
            const double radius = i % 2 ? 4 : 9;
            star << QPointF(12 + radius * std::cos(angle), 12 + radius * std::sin(angle));
        }
        p.drawPolygon(star); break;
    }
    case Symbol::Collection:
        p.drawRoundedRect(QRectF(3, 7, 18, 14), 2, 2); p.drawPolyline(QPolygonF{QPointF(3, 8), QPointF(3, 4), QPointF(10, 4), QPointF(13, 7)}); break;
    case Symbol::Queue:
        for (int y : {6, 12, 18}) { p.drawLine(9, y, 21, y); p.drawLine(3, y, 4, y); } break;
    case Symbol::Backup:
        p.drawArc(QRectF(4, 4, 16, 16), -60 * 16, 290 * 16); p.drawPolyline(QPolygonF{QPointF(2, 4), QPointF(3, 10), QPointF(9, 9)}); p.drawPolyline(QPolygonF{QPointF(12, 7), QPointF(12, 12), QPointF(16, 14)}); break;
    case Symbol::Settings:
        p.drawEllipse(QRectF(5, 5, 14, 14)); p.drawEllipse(QRectF(9, 9, 6, 6));
        for (int i = 0; i < 8; ++i) { p.save(); p.translate(12, 12); p.rotate(i * 45); p.drawLine(0, -7, 0, -10); p.restore(); } break;
    case Symbol::Search: p.drawEllipse(QRectF(4, 3, 12, 12)); p.drawLine(14, 14, 21, 21); break;
    case Symbol::Refresh: p.drawArc(QRectF(4, 4, 16, 16), 30 * 16, 300 * 16); p.drawPolyline(QPolygonF{QPointF(20, 3), QPointF(21, 9), QPointF(15, 8)}); break;
    case Symbol::Plus: p.drawLine(12, 5, 12, 19); p.drawLine(5, 12, 19, 12); break;
    case Symbol::Image: p.drawRoundedRect(QRectF(3, 3, 18, 18), 2, 2); p.drawEllipse(QRectF(14, 6, 3, 3)); p.drawPolyline(QPolygonF{QPointF(4, 18), QPointF(9, 11), QPointF(14, 17), QPointF(17, 13), QPointF(20, 17)}); break;
    case Symbol::Preview: p.drawEllipse(QRectF(4, 3, 12, 12)); p.drawLine(14, 14, 21, 21); p.drawLine(7, 9, 13, 9); p.drawLine(10, 6, 10, 12); break;
    case Symbol::Left: p.drawPolyline(QPolygonF{QPointF(15, 5), QPointF(8, 12), QPointF(15, 19)}); break;
    case Symbol::Right: p.drawPolyline(QPolygonF{QPointF(9, 5), QPointF(16, 12), QPointF(9, 19)}); break;
    case Symbol::Panel: p.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2); p.drawLine(14, 4, 14, 20); break;
    case Symbol::Check: p.drawPolyline(QPolygonF{QPointF(5, 12), QPointF(10, 17), QPointF(20, 6)}); break;
    case Symbol::Cover: p.drawRoundedRect(QRectF(6, 3, 12, 18), 2, 2); p.drawLine(9, 6, 15, 6); p.drawLine(9, 18, 13, 18); break;
    case Symbol::Wide: p.drawRoundedRect(QRectF(3, 6, 18, 12), 2, 2); p.drawLine(6, 15, 10, 15); break;
    case Symbol::Hero: p.drawRoundedRect(QRectF(2, 5, 20, 14), 2, 2); p.drawPolyline(QPolygonF{QPointF(3, 17), QPointF(9, 9), QPointF(14, 15), QPointF(17, 11), QPointF(21, 17)}); break;
    case Symbol::Logo: { QFont font; font.setPixelSize(15); font.setBold(true); p.setFont(font); p.drawText(QRect(1, 1, 22, 22), Qt::AlignCenter, "Aa"); break; }
    case Symbol::GameIcon: p.drawRoundedRect(QRectF(4, 4, 16, 16), 4, 4); p.drawPolyline(QPolygonF{QPointF(13, 6), QPointF(8, 13), QPointF(12, 13), QPointF(11, 18), QPointF(17, 10), QPointF(13, 10), QPointF(13, 6)}); break;
    case Symbol::Close: p.drawLine(6, 6, 18, 18); p.drawLine(18, 6, 6, 18); break;
    case Symbol::Trash: p.drawLine(4, 6, 20, 6); p.drawPolyline(QPolygonF{QPointF(7, 6), QPointF(8, 21), QPointF(16, 21), QPointF(17, 6)}); p.drawPolyline(QPolygonF{QPointF(9, 5), QPointF(9, 3), QPointF(15, 3), QPointF(15, 5)}); p.drawLine(11, 10, 11, 17); p.drawLine(14, 10, 14, 17); break;
    case Symbol::Link: p.drawRoundedRect(QRectF(3, 8, 10, 7), 3, 3); p.drawRoundedRect(QRectF(11, 8, 10, 7), 3, 3); break;
    case Symbol::Play: p.drawPolygon(QPolygonF{QPointF(8, 4), QPointF(20, 12), QPointF(8, 20)}); break;
    case Symbol::Pause: p.drawLine(8, 5, 8, 19); p.drawLine(16, 5, 16, 19); break;
    case Symbol::Warning: p.drawPolygon(QPolygonF{QPointF(12, 3), QPointF(22, 21), QPointF(2, 21)}); p.drawLine(12, 9, 12, 14); p.drawPoint(12, 18); break;
    case Symbol::Save: p.drawRoundedRect(QRectF(4, 3, 16, 18), 2, 2); p.drawRect(QRectF(8, 3, 8, 6)); p.drawRect(QRectF(8, 14, 8, 7)); break;
    case Symbol::Download: p.drawLine(12, 3, 12, 16); p.drawPolyline(QPolygonF{QPointF(7, 11), QPointF(12, 16), QPointF(17, 11)}); p.drawPolyline(QPolygonF{QPointF(4, 17), QPointF(4, 21), QPointF(20, 21), QPointF(20, 17)}); break;
    }
    return QIcon(pixmap);
}
bool Ui::confirmSteamChange(QWidget *parent, const QString &title, const QString &text, bool icons, const QString &details) {
    QMessageBox box(icons ? QMessageBox::Warning : QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box.setTextFormat(Qt::PlainText); box.setDefaultButton(QMessageBox::No);
    box.button(QMessageBox::Yes)->setText("Sí, continuar"); box.button(QMessageBox::No)->setText("Cancelar");
    if (!details.isEmpty()) box.setDetailedText(details);
    if (icons) {
        auto *closed = new QCheckBox("He cerrado Steam antes de continuar"); box.setCheckBox(closed); box.button(QMessageBox::Yes)->setEnabled(false);
        QObject::connect(closed, &QCheckBox::toggled, box.button(QMessageBox::Yes), &QWidget::setEnabled);
    }
    return box.exec() == QMessageBox::Yes;
}
Ui::Symbol Ui::artSymbol(const QString &type) {
    if (type == "wide") return Symbol::Wide;
    if (type == "hero") return Symbol::Hero;
    if (type == "logo") return Symbol::Logo;
    if (type == "icon") return Symbol::GameIcon;
    return Symbol::Cover;
}
QString Ui::stylesheet() {
    return QStringLiteral(R"(
    QWidget { background: #11161f; color: #e6edf6; font-family: 'Noto Sans'; font-size: 12px; }
    QFrame#sidebar { background: #0c1119; border-right: 1px solid #25303d; }
    QLabel { background: transparent; }
    QLabel#brand { color: #74e6cc; font-size: 17px; font-weight: 800; letter-spacing: 1px; }
    QLabel#eyebrow { color: #8c9fb6; font-size: 10px; font-weight: 600; letter-spacing: 1px; }
    QLabel#heading { font-size: 25px; font-weight: 750; }
    QLabel#sectionTitle { font-size: 19px; font-weight: 700; color: #9ee9d8; padding-top: 8px; }
    QScrollArea#workspaceScroll, QWidget#unifiedWorkspace { background: #11161f; }
    QLabel#muted { color: #9bacc0; }
    QLabel#gameTitle { font-size: 19px; font-weight: 700; }
    QLabel#notice { color: #b3cfc9; background: #152927; border: 1px solid #26443f; border-radius: 9px; padding: 9px 13px; }
    QLabel#errorNotice { color: #f0b8aa; background: #30232c; border: 1px solid #60424c; border-radius: 8px; padding: 8px 12px; }
    QLabel#emptyState { color: #9bacc0; padding: 28px; font-size: 14px; }
    QLabel#queueSummary { color: #9bacc0; font-size: 13px; padding: 4px 0; }
    QLabel#selectionInfo { color: #a9bdcf; font-size: 11px; }
    QFrame#surface { background: #171f2b; border: 1px solid #2b3848; border-radius: 12px; }
    QFrame#surface QLabel, QFrame#surface QWidget { background: transparent; }
    QPushButton { background: #202b3a; border: 1px solid #364559; border-radius: 8px; padding: 8px 11px; font-weight: 600; }
    QPushButton:hover { background: #2a394c; border-color: #627f97; }
    QPushButton:pressed { background: #354b61; }
    QPushButton:focus, QComboBox:focus { border: 1px solid #71e6cb; }
    QPushButton:checked { background: #294139; border-color: #658e73; }
    QPushButton:disabled { color: #718096; background: #18212c; border-color: #293443; }
    QPushButton#primary, QPushButton[intent="primary"] { color: #09251f; background: #71e6cb; border-color: #82efd5; font-weight: 750; }
    QPushButton#primary:hover, QPushButton[intent="primary"]:hover { background: #9af1dc; }
    QPushButton#primary:disabled, QPushButton[intent="primary"]:disabled { background: #25483f; color: #92b2a8; border-color: #345d51; }
    QPushButton#nav { text-align: left; padding: 12px 13px; background: transparent; border-color: transparent; color: #a1b1c6; }
    QPushButton#nav:checked { background: #1c3433; color: #92efd8; border-color: #30544c; }
    QPushButton#nav:hover { background: #1e2b3b; color: #eef5ff; }
    QPushButton#segment { padding: 7px 6px; background: #171f2b; color: #a6b7cc; font-size: 11px; }
    QPushButton#segment:checked { background: #24443e; color: #9af1dc; border-color: #4e8574; }
    QLineEdit, QComboBox { background: #17202d; border: 1px solid #344357; border-radius: 8px; padding: 8px 10px; selection-background-color: #326b61; }
    QLineEdit:focus { border-color: #71e6cb; }
    QLabel#typeNotice { color: #dac796; background: #29271d; border: 1px solid #514b30; border-radius: 7px; padding: 6px 9px; font-size: 11px; }
    QComboBox::drop-down { border: none; width: 20px; }
    QComboBox QAbstractItemView { background: #1c2939; selection-background-color: #30534d; }
    QListView { background: transparent; border: none; outline: none; }
    QListView::item:selected { background: #23443e; color: #c4faed; }
    QListWidget { background: #141d29; border: 1px solid #2c3a4e; border-radius: 10px; padding: 8px; }
    QListWidget::item { padding: 15px 12px; border-bottom: 1px solid #293747; border-radius: 5px; }
    QListWidget::item:selected { background: #23443e; color: #c4faed; }
    QCheckBox { spacing: 6px; color: #acbdd1; background: transparent; }
    QCheckBox::indicator { width: 14px; height: 14px; border-radius: 4px; border: 1px solid #647d93; background: #182330; }
    QCheckBox::indicator:checked { background: #71e6cb; image: url(:/studio/check.svg); border-color: #71e6cb; }
    QCheckBox:focus { color: #a1f6df; }
    QCheckBox:disabled { color: #738397; }
    QCheckBox::indicator:disabled { background: #273341; border-color: #4a5868; image: none; }
    QProgressBar { background: #233343; border: none; border-radius: 2px; max-height: 4px; }
    QProgressBar::chunk { background: #71e6cb; }
    QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
    QScrollBar::handle:vertical { background: #43556b; min-height: 32px; border-radius: 4px; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QSplitter::handle { background: #2c3b4c; }
    QToolTip { background: #26384a; color: #f1f6ff; border: 1px solid #607e96; padding: 7px; }
    QMenu { background: #192534; border: 1px solid #3e5369; padding: 6px; }
    QMenu::item { padding: 9px 24px; border-radius: 5px; }
    QMenu::item:selected { background: #2d5148; }
    QDialog { background: #111a26; }
    QTextEdit { background: #152131; border: 1px solid #3a4d66; border-radius: 8px; }
    )");
}
CardView::CardView(QWidget *parent) : QListView(parent) {
    setViewMode(QListView::IconMode); setResizeMode(QListView::Adjust);
    setMovement(QListView::Static); setUniformItemSizes(true); setMouseTracking(true);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}
void CardView::setEmptyState(const QString &title, const QString &description) {
    m_emptyTitle = title; m_emptyDescription = description; viewport()->update();
    setAccessibleDescription(title + ". " + description);
}
void CardView::paintEvent(QPaintEvent *event) {
    QListView::paintEvent(event);
    if (model() && model()->rowCount() > 0) return;
    QPainter p(viewport());
    const QRect area = viewport()->rect().adjusted(24, 16, -24, -16);
    const bool showIcon = area.height() >= 230;
    const int y = area.top() + qMax(0, (area.height() - (showIcon ? 220 : 160)) / 2);
    if (showIcon) Ui::icon(Ui::Symbol::Image, QColor("#5f968c")).paint(&p, QRect(area.center().x() - 18, y, 36, 36));
    const int titleTop = y + (showIcon ? 46 : 0);
    QFont title = font(); title.setPixelSize(15); title.setBold(true); p.setFont(title); p.setPen(QColor("#d8e5f0"));
    p.drawText(QRect(area.left(), titleTop, area.width(), 44), Qt::AlignHCenter | Qt::TextWordWrap, m_emptyTitle);
    QFont body = font(); body.setPixelSize(12); p.setFont(body); p.setPen(QColor("#9aafc4"));
    p.drawText(QRect(area.left(), titleTop + 48, area.width(), qMax(0, area.bottom() - titleTop - 48)), Qt::AlignHCenter | Qt::TextWordWrap, m_emptyDescription);
}
ArtworkTile::ArtworkTile(ThumbnailLoader *loader, QWidget *parent) : QWidget(parent), m_loader(loader) {
    setFixedSize(68, 96);
    connect(loader, &ThumbnailLoader::loaded, this, qOverload<>(&ArtworkTile::update));
}
void ArtworkTile::setFile(const QString &file) { m_file = file; update(); }
void ArtworkTile::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setPen(QColor("#35495c")); p.setBrush(QColor("#101923")); p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 7, 7);
    const QImage image = m_loader->image(m_file, size() * 2);
    if (image.isNull()) Ui::icon(Ui::Symbol::Image).paint(&p, QRect(22, 36, 24, 24));
    else {
        const QSize fitted = image.size().scaled(size() - QSize(8, 8), Qt::KeepAspectRatio);
        p.drawImage(QRect(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2), fitted), image);
    }
}
}
