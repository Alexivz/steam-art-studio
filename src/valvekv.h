#pragma once
#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QVariantMap>

namespace Studio {
class ValveKv {
public:
    static QVariantMap text(const QByteArray &data, QString *error = nullptr);
    static QVariantMap binary(const QByteArray &data, const QStringList &keys = {}, QString *error = nullptr);
    // Replace only icon strings; every other byte, including unknown keys and
    // launch options, stays untouched. Reject malformed/ambiguous files.
    static QByteArray shortcutIcons(const QByteArray &data, const QMap<quint32, QString> &icons, QString *error = nullptr);
    static QMap<quint32, QVariantMap> appInfo(const QByteArray &data, QString *error = nullptr);
    static QVariant value(const QVariantMap &map, const QString &key);
    static QVariantMap child(const QVariantMap &map, const QString &key);
};
}
