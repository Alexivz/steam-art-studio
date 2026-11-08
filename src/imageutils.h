#pragma once
#include <QImageReader>

namespace Studio {
// ICO contains resolutions, not animation frames. Decode the largest safe
// entry instead of magnifying the first (often only 16×16).
inline bool selectLargestIcon(QImageReader &reader) {
    if (reader.format().toLower() != "ico") return true;
    int best = -1; qint64 largest = 0;
    for (int i = 0; i < qMin(reader.imageCount(), 256); ++i) {
        if (!reader.jumpToImage(i)) continue;
        const QSize size = reader.size(); const qint64 pixels = static_cast<qint64>(size.width()) * size.height();
        if (size.isValid() && pixels <= 80LL * 1000 * 1000 && pixels > largest) { largest = pixels; best = i; }
    }
    return best >= 0 && reader.jumpToImage(best);
}
}
