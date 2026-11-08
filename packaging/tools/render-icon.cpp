#include <QGuiApplication>
#include <QIcon>
#include <QPixmap>
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (argc != 3) return 2;
    const QIcon icon(QString::fromLocal8Bit(argv[1]));
    return !icon.isNull() && icon.pixmap(256, 256).save(QString::fromLocal8Bit(argv[2]), "PNG") ? 0 : 1;
}
