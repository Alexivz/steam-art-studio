#include "credentials.h"
#include <QtGlobal>
#if defined(Q_OS_LINUX) && defined(QT_DBUS_LIB)
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#endif
#include <QRegularExpression>
#include <QStandardPaths>

namespace Studio {
QString Credentials::service() {
#if defined(Q_OS_LINUX) && defined(QT_DBUS_LIB)
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return {};
    for (const auto &name : {QString("org.kde.kwalletd6"), QString("org.kde.kwalletd5")}) {
        if (bus.interface()->isServiceRegistered(name).value()) return name;
    }
    if (!QStandardPaths::findExecutable("kwalletd6").isEmpty()) return "org.kde.kwalletd6";
    if (!QStandardPaths::findExecutable("kwalletd5").isEmpty()) return "org.kde.kwalletd5";
#endif
    return {};
}
bool Credentials::available() { return !service().isEmpty(); }
bool Credentials::valid(const QString &key) { return QRegularExpression("^[A-Za-z0-9_.-]{16,256}$").match(key).hasMatch(); }
bool Credentials::store(const QString &key, QString *error) {
#if defined(Q_OS_LINUX) && defined(QT_DBUS_LIB)
    const auto name = service();
    if (name.isEmpty()) { if (error) *error = "KWallet no está disponible. La clave solo se usará en esta sesión."; return false; }
    QDBusInterface wallet(name, name.endsWith('6') ? "/modules/kwalletd6" : "/modules/kwalletd5", "org.kde.KWallet", QDBusConnection::sessionBus());
    QDBusReply<QString> network = wallet.call("networkWallet");
    QDBusReply<int> opened = wallet.call("open", network.isValid() ? network.value() : "kdewallet", static_cast<qlonglong>(0), "Steam Art Studio");
    if (!opened.isValid() || opened.value() < 0) { if (error) *error = "No se pudo abrir KWallet. La clave solo se usará en esta sesión."; return false; }
    const int handle = opened.value();
    wallet.call("createFolder", handle, "Steam Art Studio", "Steam Art Studio");
    QDBusReply<int> saved = wallet.call("writePassword", handle, "Steam Art Studio", "steamgriddb-api-key", key, "Steam Art Studio");
    wallet.call("close", handle, false, "Steam Art Studio");
    if (!saved.isValid() || saved.value() != 0) { if (error) *error = "No se pudo guardar la clave en KWallet."; return false; }
    return true;
#else
    Q_UNUSED(key);
    if (error) *error = "KWallet no está disponible en esta plataforma. La clave permanece solo en memoria durante esta sesión.";
    return false;
#endif
}
QString Credentials::load(QString *error) {
#if defined(Q_OS_LINUX) && defined(QT_DBUS_LIB)
    const auto name = service(); if (name.isEmpty()) return {};
    QDBusInterface wallet(name, name.endsWith('6') ? "/modules/kwalletd6" : "/modules/kwalletd5", "org.kde.KWallet", QDBusConnection::sessionBus());
    QDBusReply<QString> network = wallet.call("networkWallet");
    QDBusReply<int> opened = wallet.call("open", network.isValid() ? network.value() : "kdewallet", static_cast<qlonglong>(0), "Steam Art Studio");
    if (!opened.isValid() || opened.value() < 0) { if (error) *error = "No se pudo abrir KWallet."; return {}; }
    QDBusReply<QString> result = wallet.call("readPassword", opened.value(), "Steam Art Studio", "steamgriddb-api-key", "Steam Art Studio");
    wallet.call("close", opened.value(), false, "Steam Art Studio");
    return result.isValid() && valid(result.value()) ? result.value() : QString();
#else
    Q_UNUSED(error);
    return {};
#endif
}
}
