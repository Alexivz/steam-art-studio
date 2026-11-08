#pragma once
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

namespace Studio {
// Deliberately accepts counts only, never settings, paths, keys or game objects.
inline QByteArray diagnosticReport(qint64 games, qint64 cacheBytes, int pending, int downloads, bool ffmpeg, bool demo) {
    const QJsonObject report{{"format", 1}, {"application", "Steam Art Studio"}, {"version", QCoreApplication::applicationVersion()},
        {"qt", QString(qVersion())}, {"system", QSysInfo::kernelType()}, {"architecture", QSysInfo::currentCpuArchitecture()},
        {"local_entries", games}, {"cache_bytes", cacheBytes}, {"pending_changes", pending}, {"active_downloads", downloads},
        {"ffmpeg_available", ffmpeg}, {"demo", demo},
        {"privacy", "No keys, account identifiers, personal paths, URLs, game lists or logs. Nothing is uploaded."}};
    return QJsonDocument(report).toJson(QJsonDocument::Indented);
}
}
