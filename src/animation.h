#pragma once
#include <QString>

namespace Studio {
enum class AnimationProfile { Original, Balanced, Small };
struct AnimationLimits {
    qint64 maximumBytes = 64LL * 1024 * 1024;
    int timeoutMs = 120000;
};
// Local conversion only. A non-original profile must be selected explicitly.
bool convertAnimation(const QString &source, const QString &destination, const QString &type,
                      AnimationProfile profile, QString *error, const AnimationLimits &limits = {});
QString animationProfileTitle(AnimationProfile profile);
}
