#include "steam.h"
#include "valvekv.h"
#include "platform.h"
#ifdef Q_OS_WIN
#include <QSettings>
#endif
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace Studio {
namespace {
QByteArray readBounded(const QString &path, qint64 maximum = 64 * 1024 * 1024) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > maximum) return {};
    return file.readAll();
}
QVariantMap readText(const QString &path) { return ValveKv::text(readBounded(path)); }
QString pickFile(const QString &directory, const QString &base) {
    for (const auto &extension : {"png", "jpg", "jpeg", "webp", "gif", "ico"}) {
        const QString path = directory + "/" + base + "." + extension;
        if (QFileInfo(path).isFile()) return path;
    }
    return {};
}
QString assetPath(const QString &root, quint32 appId, const QVariantMap &common, const QString &kind) {
    const QString cache = root + "/appcache/librarycache";
    const QString id = QString::number(appId);
    const auto full = ValveKv::child(common, "library_assets_full");
    const auto entry = ValveKv::child(full, kind);
    const auto images = ValveKv::child(entry, "image");
    QString name = ValveKv::value(images, "english").toString();
    if (name.isEmpty() && !images.isEmpty()) name = images.cbegin().value().toString();
    if (!name.isEmpty() && QFileInfo(name).fileName() == name) {
        const QString path = cache + "/" + id + "/" + name;
        if (QFileInfo(path).isFile()) return path;
    }
    QString base = kind == "library_capsule" ? "library_600x900" : kind;
    for (const auto &candidate : {base, kind}) {
        QString path = pickFile(cache, id + "_" + candidate);
        if (!path.isEmpty()) return path;
        path = pickFile(cache + "/" + id, candidate);
        if (!path.isEmpty()) return path;
    }
    return {};
}
}
QVector<SteamRoot> SteamScanner::discover(const QString &homeArg) {
    const QString home = homeArg.isEmpty() ? QDir::homePath() : homeArg;
    QStringList roots = {home + "/.local/share/Steam", home + "/.steam/steam", home + "/.steam/root",
        home + "/.var/app/com.valvesoftware.Steam/data/Steam", home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam"};
#ifdef Q_OS_WIN
    if (homeArg.isEmpty()) {
        QStringList registered;
        QSettings user("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat); registered << user.value("SteamPath").toString();
        for (const auto &key : {"HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam", "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam"}) {
            QSettings machine(QString::fromLatin1(key), QSettings::NativeFormat); registered << machine.value("InstallPath").toString();
        }
        roots = Platform::windowsSteamCandidates(registered, qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)"));
    }
#endif
    QVector<SteamRoot> result; QSet<QString> seen;
    for (const auto &path : roots) {
        const auto canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty() || seen.contains(Platform::pathKey(canonical)) || !QFileInfo(canonical + "/appcache").isDir()) continue;
        seen.insert(Platform::pathKey(canonical));
        result.append({canonical, path.contains("com.valvesoftware.Steam") ? QStringLiteral("Steam · Flatpak") : QStringLiteral("Steam · nativo")});
    }
    return result;
}
QString SteamScanner::gridDirectory(const Game &game) {
    if (!QRegularExpression("^[0-9]+$").match(game.userId).hasMatch()) return {};
    return game.steamRoot + "/userdata/" + game.userId + "/config/grid";
}
QString SteamScanner::existingArt(const Game &game, const QString &type) {
    if (type == "icon") {
        if (game.external) {
            QString path = game.icon.trimmed();
            if (path.startsWith('"') && path.endsWith('"')) path = path.mid(1, path.size() - 2);
            if (QDir::isAbsolutePath(path) && QFileInfo(path).isFile()
                && QStringList{"png", "jpg", "jpeg", "ico", "webp", "gif", "apng"}.contains(QFileInfo(path).suffix().toLower())) return path;
        } else {
            if (game.iconHash.size() != 40 || !QRegularExpression("^[a-fA-F0-9]{40}$").match(game.iconHash).hasMatch()) return {};
            const QString directory = game.steamRoot + "/appcache/librarycache";
            const QString nested = directory + "/" + QString::number(game.appId) + "/" + game.iconHash.toLower() + ".jpg";
            if (QFileInfo(nested).isFile()) return nested;
            const QString legacy = directory + "/" + QString::number(game.appId) + "_icon.jpg";
            return QFileInfo(legacy).isFile() ? legacy : QString();
        }
    }
    const auto directory = gridDirectory(game); const auto base = artBase(game.appId, type);
    return directory.isEmpty() || base.isEmpty() ? QString() : pickFile(directory, base);
}
ScanResult SteamScanner::scan(const QString &rootArg, const QString &requestedUser) {
    ScanResult result; result.root = QFileInfo(rootArg).canonicalFilePath();
    if (result.root.isEmpty()) { result.error = "No se encuentra esta instalación de Steam."; return result; }
    const QString root = result.root;
    const auto users = ValveKv::child(readText(root + "/config/loginusers.vdf"), "users");
    QString selected;
    if (!requestedUser.isEmpty()) selected = requestedUser;
    else {
        for (auto it = users.cbegin(); it != users.cend(); ++it) {
            bool valid = false; const auto id64 = it.key().toULongLong(&valid);
            if (!valid || id64 < 76561197960265728ULL) continue;
            const auto user = it.value().toMap();
            if (selected.isEmpty() || ValveKv::value(user, "MostRecent").toString() == "1") {
                selected = QString::number(id64 - 76561197960265728ULL);
                if (ValveKv::value(user, "MostRecent").toString() == "1") break;
            }
        }
    }
    if (!QRegularExpression("^[0-9]+$").match(selected).hasMatch()) {
        result.error = "Inicia sesión en Steam al menos una vez para detectar la cuenta local."; return result;
    }
    result.userId = selected;
    const QString config = root + "/userdata/" + selected + "/config";
    result.watchedPaths = {root + "/appcache/appinfo.vdf", root + "/steamapps/libraryfolders.vdf",
        root + "/config/loginusers.vdf", config + "/localconfig.vdf", config + "/shortcuts.vdf",
        config + "/librarycache", config + "/grid", root + "/appcache/librarycache"};

    QVariantMap local = readText(config + "/localconfig.vdf");
    if (ValveKv::value(local, "UserLocalConfigStore").isValid()) local = ValveKv::child(local, "UserLocalConfigStore");
    for (const auto &key : {"Software", "Valve", "Steam", "apps"}) local = ValveKv::child(local, key);
    QSet<quint32> known; QSet<quint32> direct; QSet<quint32> installed;
    QMap<quint32, QString> manifestNames;
    for (auto it = local.cbegin(); it != local.cend(); ++it) {
        bool ok = false; auto id = it.key().toUInt(&ok); if (ok && id) { known.insert(id); direct.insert(id); }
    }
    QStringList libraries = {root};
    auto folders = ValveKv::child(readText(root + "/steamapps/libraryfolders.vdf"), "libraryfolders");
    for (auto it = folders.cbegin(); it != folders.cend(); ++it) {
        const auto map = it.value().toMap();
        const auto path = ValveKv::value(map, "path").toString();
        if (!path.isEmpty() && QFileInfo(path + "/steamapps").isDir()) libraries.append(path);
        auto apps = ValveKv::child(map, "apps");
        for (auto app = apps.cbegin(); app != apps.cend(); ++app) { bool ok = false; auto id = app.key().toUInt(&ok); if (ok && id) { known.insert(id); direct.insert(id); installed.insert(id); } }
    }
    libraries.removeDuplicates();
    for (const auto &library : libraries) {
        const QString directory = library + "/steamapps";
        result.watchedPaths.append(directory);
        const auto manifests = QDir(directory).entryList({"appmanifest_*.acf"}, QDir::Files);
        for (const auto &filename : manifests) {
            const auto state = ValveKv::child(readText(directory + "/" + filename), "AppState");
            bool ok = false; const auto id = ValveKv::value(state, "appid").toUInt(&ok);
            if (ok && id) { known.insert(id); direct.insert(id); installed.insert(id); manifestNames[id] = ValveKv::value(state, "name").toString(); }
        }
    }
    // Cached library assets can represent an uninstalled game not returned by
    // an online ownership API. Mark such entries as cached, not as licensed.
    for (const auto &directory : {root + "/appcache/librarycache", config + "/librarycache"}) {
        for (const auto &entry : QDir(directory).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            const auto prefix = entry.fileName().section('_', 0, 0).section('.', 0, 0);
            bool ok = false; const auto id = prefix.toUInt(&ok); if (ok && id) known.insert(id);
        }
    }
    QString parseError;
    const auto metadata = ValveKv::appInfo(readBounded(root + "/appcache/appinfo.vdf", 256 * 1024 * 1024), &parseError);
    if (!parseError.isEmpty()) result.warnings.append("Los metadatos de Steam están cambiando o no se pudieron leer; se reintentará.");
    for (auto id : known) {
        const auto common = ValveKv::child(metadata.value(id), "common");
        const QString kind = ValveKv::value(common, "type").toString().toLower();
        if (!kind.isEmpty() && kind != "game" && kind != "demo" && kind != "beta" && kind != "application") continue;
        QString name = ValveKv::value(common, "name").toString();
        if (name.isEmpty()) name = manifestNames.value(id);
        if (name.isEmpty()) continue;
        Game game; game.appId = id; game.name = name; game.steamRoot = root; game.userId = selected;
        game.installed = installed.contains(id); game.cachedOnly = !direct.contains(id); game.kind = kind.isEmpty() ? "game" : kind;
        game.lastPlayed = ValveKv::value(local.value(QString::number(id)).toMap(), "LastPlayed").toLongLong();
        game.cover = existingArt(game, "capsule"); if (game.cover.isEmpty()) game.cover = assetPath(root, id, common, "library_capsule");
        game.hero = existingArt(game, "hero"); if (game.hero.isEmpty()) game.hero = assetPath(root, id, common, "library_hero");
        const QString iconHash = ValveKv::value(common, "clienticon").toString().toLower();
        if (iconHash.size() == 40 && QRegularExpression("^[a-f0-9]{40}$").match(iconHash).hasMatch()) game.iconHash = iconHash;
        game.icon = existingArt(game, "icon"); if (!game.icon.isEmpty()) result.watchedPaths.append(game.icon);
        result.games.append(game);
    }
    const auto shortcutData = readBounded(config + "/shortcuts.vdf", 16 * 1024 * 1024);
    if (!shortcutData.isEmpty()) {
        QString error; const auto shortcuts = ValveKv::child(ValveKv::binary(shortcutData, {}, &error), "shortcuts");
        if (!error.isEmpty()) result.warnings.append("No se pudieron leer algunos accesos externos.");
        for (auto it = shortcuts.cbegin(); it != shortcuts.cend(); ++it) {
            const auto item = it.value().toMap(); bool ok = false;
            const quint32 rawId = ValveKv::value(item, "appid").toUInt(&ok);
            const quint32 id = rawId | 0x80000000U;
            const QString name = ValveKv::value(item, "AppName").toString();
            if (!ok || !rawId || name.isEmpty()) continue;
            Game game; game.appId = id; game.name = name; game.external = true; game.installed = true;
            game.steamRoot = root; game.userId = selected; game.lastPlayed = ValveKv::value(item, "LastPlayTime").toLongLong();
            game.icon = ValveKv::value(item, "icon").toString();
            game.cover = existingArt(game, "capsule"); game.hero = existingArt(game, "hero"); result.games.append(game);
        }
    }
    std::sort(result.games.begin(), result.games.end(), [](const Game &a, const Game &b) { return a.name.localeAwareCompare(b.name) < 0; });
    result.watchedPaths.removeDuplicates(); return result;
}
QString SteamScanner::signature(const QStringList &paths) {
    QByteArray data;
    for (const auto &path : paths) {
        QFileInfo info(path); data += path.toUtf8() + '\0' + QByteArray::number(info.size()) + ':' + QByteArray::number(info.lastModified().toMSecsSinceEpoch()) + ';';
    }
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
}
