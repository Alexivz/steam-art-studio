#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Studio::Platform {
inline Qt::CaseSensitivity pathCase() {
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}
inline QString pathKey(const QString &path) {
    const auto clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    return pathCase() == Qt::CaseInsensitive ? clean.toCaseFolded() : clean;
}
inline bool sameOrBelow(QString path, QString root, Qt::CaseSensitivity sensitivity = pathCase()) {
    path = QDir::cleanPath(path); root = QDir::cleanPath(root);
    if (path.compare(root, sensitivity) == 0) return true;
    if (!root.endsWith('/')) root += '/';
    return path.startsWith(root, sensitivity);
}
inline bool isLink(const QFileInfo &info) {
    if (info.isSymLink()) return true;
#ifdef Q_OS_WIN
    const auto path = QDir::toNativeSeparators(info.absoluteFilePath());
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return false;
#endif
}
inline QString dataDirectory(bool demo) {
    auto base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + (demo ? "-demo" : "");
#ifdef Q_OS_WIN
    return base + "/data";
#else
    return base;
#endif
}
inline QString cacheDirectory(bool demo) {
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + (demo ? "-demo" : "") + "/cache";
#else
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + (demo ? "-demo" : "");
#endif
}
inline QString findTool(const QString &name) {
    if (name != "ffmpeg") return {};
#ifdef Q_OS_WIN
    const QString executable = name + ".exe";
#else
    const QString executable = name;
#endif
    const auto base = QCoreApplication::applicationDirPath();
    for (const auto &path : {base + "/" + executable, base + "/../libexec/steam-art-studio/" + executable}) {
        const QFileInfo info(path); if (info.isFile() && info.isExecutable()) return info.absoluteFilePath();
    }
    // Do not add the current working directory to executable lookup on Windows.
    QStringList paths;
    for (const auto &path : qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts)) if (QDir::isAbsolutePath(path)) paths.append(path);
    return paths.isEmpty() ? QString() : QStandardPaths::findExecutable(executable, paths);
}
inline QStringList windowsSteamCandidates(const QStringList &registered, const QString &programFiles, const QString &programFilesX86) {
    QStringList paths = registered;
    for (const auto &base : {programFilesX86, programFiles}) if (!base.isEmpty()) paths.append(base + "/Steam");
    QStringList result;
    for (auto path : paths) {
        path.replace('\\', '/'); path = QDir::cleanPath(path);
        if (path.isEmpty() || path == "." || result.contains(path, Qt::CaseInsensitive)) continue;
        result.append(path);
    }
    return result;
}
}
