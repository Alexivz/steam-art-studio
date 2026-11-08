#pragma once
#include "types.h"

namespace Studio {
class SteamScanner {
public:
    static QVector<SteamRoot> discover(const QString &home = {});
    static ScanResult scan(const QString &root, const QString &requestedUser = {});
    static QString signature(const QStringList &paths);
    static QString gridDirectory(const Game &game);
    static QString existingArt(const Game &game, const QString &type);
};
}
