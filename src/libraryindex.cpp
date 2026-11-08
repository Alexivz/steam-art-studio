#include "libraryindex.h"
#include <QCollator>
#include <algorithm>
#include <numeric>

namespace Studio {
QString LibraryIndex::normalize(const QString &text) {
    QString result; result.reserve(text.size());
    for (const QChar c : text.normalized(QString::NormalizationForm_D).toCaseFolded())
        if (c.category() != QChar::Mark_NonSpacing) result += c;
    return result;
}
void LibraryIndex::setGames(const QVector<Game> &games) {
    m_games = games; m_names.clear(); m_keys.clear(); m_names.reserve(games.size()); m_keys.reserve(games.size());
    for (const auto &game : games) { m_names.append(normalize(game.name)); m_keys.append(game.key()); }
    auto &names = m_order[0]; names.resize(games.size()); std::iota(names.begin(), names.end(), 0);
    // Qt's C-locale backend ignores numeric collation (common in containers).
    const QLocale locale;
    QCollator collator(locale.language() == QLocale::C ? QLocale(QLocale::English, QLocale::UnitedStates) : locale);
    collator.setCaseSensitivity(Qt::CaseInsensitive); collator.setNumericMode(true);
    std::stable_sort(names.begin(), names.end(), [&](int a, int b) {
        const int order = collator.compare(games[a].name, games[b].name);
        return order != 0 ? order < 0 : m_keys[a] < m_keys[b];
    });
    m_order[1] = names; std::reverse(m_order[1].begin(), m_order[1].end());
    m_order[2] = names; std::stable_sort(m_order[2].begin(), m_order[2].end(), [&](int a, int b) { return games[a].lastPlayed > games[b].lastPlayed; });
    m_order[3] = names; std::stable_sort(m_order[3].begin(), m_order[3].end(), [&](int a, int b) { return games[a].installed > games[b].installed; });
}
QVector<Game> LibraryIndex::query(const LibraryFilter &filter, qint64 now) const {
    const QString text = normalize(filter.text); QVector<Game> result; result.reserve(m_games.size());
    for (int i : m_order[qBound(0, filter.order, 3)]) {
        const auto &game = m_games[i];
        if (!text.isEmpty() && !m_names[i].contains(text)) continue;
        if (filter.favoritesOnly && !filter.favorites.contains(m_keys[i])) continue;
        if (filter.collectionOnly && !filter.collection.contains(m_keys[i])) continue;
        if (filter.status == 1 && !game.installed) continue;
        if (filter.status == 2 && !game.external) continue;
        if (filter.status == 3 && game.lastPlayed < now - 30LL * 86400) continue;
        result.append(game);
    }
    return result;
}
}
