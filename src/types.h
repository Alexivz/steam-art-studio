#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

namespace Studio {
inline QStringList artTypes() { return {"capsule", "wide", "hero", "logo", "icon"}; }
inline QString artTitle(const QString &type) {
    if (type == "wide") return QStringLiteral("Horizontal");
    if (type == "hero") return QStringLiteral("Fondo");
    if (type == "logo") return QStringLiteral("Logotipo");
    if (type == "icon") return QStringLiteral("Icono");
    return QStringLiteral("Carátula");
}
inline QString artBase(quint32 appId, const QString &type) {
    const QString id = QString::number(appId);
    if (type == "capsule") return id + "p";
    if (type == "wide") return id;
    if (type == "hero") return id + "_hero";
    if (type == "logo") return id + "_logo";
    if (type == "icon") return id + "_icon";
    return {};
}
struct SteamRoot {
    QString path;
    QString label;
};
struct Game {
    quint32 appId = 0;
    QString name;
    QString steamRoot;
    QString userId;
    QString cover;
    QString hero;
    QString icon;
    QString iconHash;
    QString kind = "game";
    bool installed = false;
    bool external = false;
    bool cachedOnly = false;
    qint64 lastPlayed = 0;
    QString key() const { return steamRoot + "|" + userId + "|" + QString::number(appId); }
};
struct ScanResult {
    QVector<Game> games;
    QStringList watchedPaths;
    QString root;
    QString userId;
    QString error;
    QStringList warnings;
};
struct Artwork {
    QString id;
    QString author;
    QString type;
    QUrl thumbnail;
    QUrl original;
    QString mime;
    int width = 0;
    int height = 0;
    bool animated = false;
    bool adult = false;
};
struct PendingChange {
    Game game;
    QString type;
    QString file;
    QString description;
    bool animated = false;
    QString key() const { return game.key() + "|" + type; }
};
struct TransactionResult {
    bool ok = false;
    QString id;
    QString error;
    int files = 0;
};
}
Q_DECLARE_METATYPE(Studio::Game)
Q_DECLARE_METATYPE(Studio::ScanResult)
Q_DECLARE_METATYPE(Studio::Artwork)
Q_DECLARE_METATYPE(QVector<Studio::Artwork>)
Q_DECLARE_METATYPE(Studio::TransactionResult)
