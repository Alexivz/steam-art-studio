#include "animation.h"
#include "platform.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QSize>
#include <QStandardPaths>

namespace Studio {
QString animationProfileTitle(AnimationProfile profile) {
    if (profile == AnimationProfile::Balanced) return "Equilibrada · hasta 20 FPS y resolución limitada";
    if (profile == AnimationProfile::Small) return "Ligera · hasta 12 FPS y resolución reducida";
    return "Original · Máxima calidad · sin reducir resolución ni fotogramas";
}
bool convertAnimation(const QString &source, const QString &destination, const QString &type,
                      AnimationProfile profile, QString *error, const AnimationLimits &limits) {
    auto fail = [error](const QString &message) { if (error) *error = message; return false; };
    if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink()) return fail("El destino temporal ya existe; no se sobrescribirá.");
    if (limits.maximumBytes <= 0 || limits.timeoutMs <= 0) return fail("Los límites de conversión no son válidos.");
    const auto ffmpeg = Platform::findTool("ffmpeg");
    if (ffmpeg.isEmpty()) return fail("Instala FFmpeg para convertir esta animación a APNG compatible con Steam.");
    QStringList arguments = {"-v", "error", "-nostdin", "-xerror", "-threads", "1", "-protocol_whitelist", "file,pipe", "-i", source};
    if (profile != AnimationProfile::Original) {
        QSize bound = type == "hero" ? QSize(1920, 620) : type == "wide" ? QSize(920, 430) : type == "logo" ? QSize(1280, 720) : QSize(600, 900);
        if (profile == AnimationProfile::Small) bound = type == "hero" ? QSize(1280, 414) : type == "wide" ? QSize(640, 300) : type == "logo" ? QSize(854, 480) : QSize(400, 600);
        // Frame selection, never interpolation. No cropping, upscaling or loss of alpha.
        arguments << "-vf" << QString("select='isnan(prev_selected_t)+gte(t-prev_selected_t,1/%1)',scale=w='min(iw,%2)':h='min(ih,%3)':force_original_aspect_ratio=decrease")
            .arg(profile == AnimationProfile::Small ? 12 : 20).arg(bound.width()).arg(bound.height());
    }
    arguments << "-fps_mode" << "passthrough" << "-threads" << "1" << "-plays" << "0" << "-f" << "apng" << "-n" << destination;
    QProcess process; process.setProcessChannelMode(QProcess::SeparateChannels);
    QElapsedTimer timer; timer.start(); process.start(ffmpeg, arguments);
    if (!process.waitForStarted(3000)) return fail("No se pudo iniciar FFmpeg para convertir la animación. No se ha modificado Steam.");
    auto stop = [&](const QString &message) {
        process.kill(); process.waitForFinished(3000); QFile::remove(destination);
        return fail(message + " No se ha modificado Steam; la cola y el original se conservan.");
    };
    QByteArray diagnostic;
    while (true) {
        const int remaining = qMax(1, limits.timeoutMs - static_cast<int>(timer.elapsed()));
        const bool finished = process.waitForFinished(qMin(50, remaining));
        diagnostic = (diagnostic + process.readAllStandardError()).right(16384);
        process.readAllStandardOutput();
        // Check both while running and after a quick successful exit.
        if (QFileInfo(destination).size() > limits.maximumBytes) {
            return stop(QString("El APNG convertido supera el límite de %1 MiB, aunque el original comprimido sea más pequeño. En Cambios, elige animaciones Equilibrada o Ligera y vuelve a confirmar.").arg(limits.maximumBytes / (1024. * 1024.), 0, 'f', 1));
        }
        if (finished || process.state() == QProcess::NotRunning) break;
        if (timer.elapsed() >= limits.timeoutMs) return stop(QString("La conversión agotó el tiempo máximo (%1 s). En Cambios, prueba animaciones Equilibrada o Ligera, o elige una animación más corta.").arg(limits.timeoutMs / 1000., 0, 'f', 1));
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QFile::remove(destination);
        if (diagnostic.contains("image data not found") || diagnostic.contains("skipping unsupported chunk"))
            return fail("Esta versión de FFmpeg no puede decodificar la animación. Para WebP animado, usa una versión compatible o un diseño APNG/GIF. No se ha modificado Steam.");
        return fail("FFmpeg no pudo convertir la animación. Comprueba el archivo, el espacio disponible y la compatibilidad del formato. No se ha modificado Steam; la cola se conserva.");
    }
    QImageReader reader(destination);
    if (QFileInfo(destination).size() < 1 || reader.format().toLower() != "png" || !reader.canRead()) {
        QFile::remove(destination); return fail("El conversor no produjo un PNG/APNG válido. No se ha modificado Steam; la cola se conserva.");
    }
    return true;
}
}
