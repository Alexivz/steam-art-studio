#pragma once
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QProcess>

namespace TestAnimation {
// Unique frames prevent the WebP encoder from coalescing identical frames:
// a 60 FPS source must really contain 30 distinct frames in this half-second.
inline bool uniqueSixtyFpsWebp(const QString &path, const QSize &size, const QString &ffmpeg) {
    const QString directory = QFileInfo(path).absolutePath() + "/" + QFileInfo(path).completeBaseName() + "-frames";
    if (QFileInfo::exists(directory) || !QDir().mkpath(directory)) return false;
    for (int i = 0; i < 30; ++i) {
        QImage image(size, QImage::Format_ARGB32); image.fill(QColor((i * 37) % 256, (i * 71) % 256, (i * 109) % 256, 128));
        if (!image.save(directory + QString("/frame-%1.png").arg(i, 3, 10, QLatin1Char('0')), "PNG")) return false;
    }
    QProcess encoder; encoder.start(ffmpeg, {"-v", "error", "-framerate", "60", "-i", directory + "/frame-%03d.png", "-c:v", "libwebp_anim", "-lossless", "1", "-loop", "0", "-y", path});
    return encoder.waitForFinished(20000) && encoder.exitCode() == 0;
}
inline QByteArray decodedRgba(const QString &path, const QString &ffmpeg) {
    QProcess process; process.start(ffmpeg, {"-v", "error", "-threads", "1", "-i", path, "-fps_mode", "passthrough", "-threads", "1", "-f", "rawvideo", "-pix_fmt", "rgba", "pipe:1"});
    return process.waitForFinished(20000) && process.exitCode() == 0 ? process.readAllStandardOutput() : QByteArray();
}
}
