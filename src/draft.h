#pragma once
#include "types.h"

namespace Studio {
struct DraftResult {
    QVector<PendingChange> changes;
    QString warning;
    int rejected = 0;
};
class DraftStore {
public:
    DraftStore(const QString &data, const QString &cache, bool demo = false);
    DraftResult load() const;
    bool save(const QVector<PendingChange> &changes, QString *error = nullptr) const;
    QStringList previousOriginals() const;
    static bool separateDirectories(const QString &data, const QString &cache);
private:
    QString m_data, m_cache;
    bool m_demo;
};
}
