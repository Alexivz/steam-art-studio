#include "libraryindex.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSysInfo>
#include <algorithm>
#include <cstdio>
#include <sys/resource.h>

using namespace Studio;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    bool ok = false; const int count = app.arguments().value(1, "10000").toInt(&ok);
    if (!ok || count < 100 || count > 100000) return 2;
    QVector<Game> games; games.reserve(count);
    for (int i = count; i > 0; --i) { Game g; g.appId = i; g.name = QString("Juego %1 — Galaxia %2").arg(i).arg(i % 23); g.steamRoot = "/synthetic/steam"; g.userId = "42"; g.installed = i % 5 == 0; g.lastPlayed = i; games.append(g); }
    QElapsedTimer timer; timer.start(); LibraryIndex index; index.setGames(games); const double build = timer.nsecsElapsed() / 1e6;
    QList<double> indexed, legacy; qint64 checksum = 0;
    for (const QString &query : {QString("juego"), QString("galaxia 1"), QString("987"), QString("galaxia"), QString("juego 2"), QString("missing"), QString("juego"), QString("galaxia 2"), QString("juego")}) {
        timer.restart(); LibraryFilter filter; filter.text = query; const auto result = index.query(filter); indexed.append(timer.nsecsElapsed() / 1e6); checksum += result.size();
        timer.restart(); QVector<Game> previous; const QString normalizedQuery = LibraryIndex::normalize(query);
        for (const auto &game : games) if (LibraryIndex::normalize(game.name).contains(normalizedQuery)) previous.append(game);
        std::stable_sort(previous.begin(), previous.end(), [](const Game &a, const Game &b) { return a.name.localeAwareCompare(b.name) < 0; });
        legacy.append(timer.nsecsElapsed() / 1e6);
        if (result.size() != previous.size()) return 3;
    }
    std::sort(indexed.begin(), indexed.end()); std::sort(legacy.begin(), legacy.end()); struct rusage usage{}; getrusage(RUSAGE_SELF, &usage);
    const QJsonObject output{{"synthetic_games", count}, {"qt", QString(qVersion())}, {"cpu_arch", QSysInfo::currentCpuArchitecture()}, {"index_build_ms", build}, {"indexed_query_median_ms", indexed[indexed.size()/2]}, {"indexed_query_max_ms", indexed.last()}, {"previous_pipeline_median_ms", legacy[legacy.size()/2]}, {"previous_pipeline_max_ms", legacy.last()}, {"peak_process_rss_kib", static_cast<double>(usage.ru_maxrss)}, {"result_count_checksum", static_cast<double>(checksum)}, {"steam_writes", false}, {"scope", "Synthetic filtering/ordering only; not rendering, network or other applications"}};
    std::fputs(QJsonDocument(output).toJson().constData(), stdout); return 0;
}
