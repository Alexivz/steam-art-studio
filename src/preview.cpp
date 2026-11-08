#include "preview.h"
#include "imageutils.h"
#include "platform.h"
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QStandardPaths>

namespace Studio {
Preview::Preview(QWidget *parent) : QWidget(parent), m_process(this) {
    setMinimumSize(300, 260);
    m_process.setObjectName("previewDecoder");
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (m_frameBytes < 1) { m_process.readAllStandardOutput(); return; }
        while (m_process.bytesAvailable() > 0) {
            const auto bytes = m_process.read(m_frameBytes - m_buffer.size());
            if (bytes.isEmpty()) break;
            m_buffer.append(bytes);
            if (m_buffer.size() == m_frameBytes) {
                m_image = QImage(reinterpret_cast<const uchar *>(m_buffer.constData()), m_frameSize.width(), m_frameSize.height(), QImage::Format_ARGB32).copy();
                m_buffer.clear(); emit frameReady(m_frameSize);
            }
        }
        update();
    });
    // Never accumulate FFmpeg's diagnostics (which can contain local paths).
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] { m_process.readAllStandardError(); });
    connect(&m_process, &QProcess::started, this, [this] { emit playbackChanged(true); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) { emit playbackChanged(false); emit this->error("No se pudo iniciar FFmpeg. Revisa su instalación."); } });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        emit playbackChanged(false); if (code && status == QProcess::NormalExit) emit error("No se pudo reproducir esta animación. La imagen estática sigue disponible.");
    });
}
Preview::~Preview() { stop(); }
void Preview::setFile(const QString &file, bool animated) {
    stop(); m_file = file; m_animated = animated;
    if (file.isEmpty()) { m_image = QImage(); update(); return; }
    QImageReader reader(file); reader.setAutoTransform(true); selectLargestIcon(reader); const auto size = reader.size();
    m_image = QImage();
    if (size.isValid() && static_cast<qint64>(size.width()) * size.height() <= 80LL * 1000 * 1000) {
        if (size.width() > 1200 || size.height() > 900) reader.setScaledSize(size.scaled(1200, 900, Qt::KeepAspectRatio));
        m_image = reader.read();
    }
    update();
}
void Preview::togglePlayback() {
    if (playing()) { stop(); return; }
    if (!m_animated || m_file.isEmpty()) return;
    QFile input(m_file); if (!input.open(QIODevice::ReadOnly) || input.size() > 64 * 1024 * 1024) { emit error("El archivo no está disponible o es demasiado grande."); return; }
    const auto header = input.read(16);
    if (!header.startsWith(QByteArray::fromHex("89504e47")) && !header.startsWith("GIF") && !header.startsWith("RIFF")
        && !header.startsWith(QByteArray::fromHex("1a45dfa3")) && header.mid(4, 4) != "ftyp") { emit error("Formato no permitido para animaciones."); return; }
    const auto ffmpeg = Platform::findTool("ffmpeg");
    if (ffmpeg.isEmpty()) { emit error("Instala FFmpeg para reproducir animaciones."); return; }
    QImageReader reader(m_file); m_frameSize = reader.size();
    const bool image = m_frameSize.isValid();
    if (!image) m_frameSize = QSize(480, 320); // Bounded fallback for video thumbnails, not applied artwork.
    const qint64 pixels = static_cast<qint64>(m_frameSize.width()) * m_frameSize.height();
    if (pixels < 1 || pixels > 16LL * 1024 * 1024) {
        emit error("La animación supera el límite de memoria de la vista previa a resolución original. No se ha reducido ni modificado el archivo; puedes conservarlo al aplicar con Original."); return;
    }
    m_frameBytes = static_cast<qsizetype>(pixels * 4); m_buffer.clear();
    QStringList arguments = {"-v", "error", "-nostdin", "-threads", "2", "-protocol_whitelist", "file,pipe", "-re", "-stream_loop", "-1", "-noautorotate", "-i", m_file, "-map", "0:v:0", "-an", "-sn", "-dn"};
    // Native GIF/APNG/WebP pixels and timing. Scaling in paintEvent affects the
    // display only; never force a 480x320 canvas or a 20 FPS conversion here.
    if (!image) arguments << "-vf" << "scale=480:320:force_original_aspect_ratio=decrease,pad=480:320:(ow-iw)/2:(oh-ih)/2:color=black@0";
    arguments << "-fps_mode" << "passthrough" << "-threads" << "1" << "-f" << "rawvideo" << "-pix_fmt" << "bgra" << "pipe:1";
    m_process.start(ffmpeg, arguments);
}
void Preview::stop() {
    if (m_process.state() != QProcess::NotRunning) { m_process.kill(); m_process.waitForFinished(1500); }
    m_buffer.clear(); emit playbackChanged(false);
}
void Preview::hideEvent(QHideEvent *event) { stop(); QWidget::hideEvent(event); }
void Preview::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::SmoothPixmapTransform); p.fillRect(rect(), QColor("#151821"));
    if (m_backgroundMode == 2) p.fillRect(rect(), QColor("#e0e5ed"));
    if (m_backgroundMode == 0) for (int y = 0; y < height(); y += 22) for (int x = 0; x < width(); x += 22) if ((x / 22 + y / 22) % 2) p.fillRect(x, y, 22, 22, QColor("#232d3b"));
    if (m_image.isNull()) { p.setPen(QColor("#8f9bb0")); p.drawText(rect(), Qt::AlignCenter, m_file.isEmpty() ? "Selecciona una imagen" : "Pulsa Reproducir para ver la animación"); return; }
    const auto size = m_image.size().scaled(this->size() - QSize(36, 36), Qt::KeepAspectRatio);
    p.drawImage(QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size), m_image);
}
}
