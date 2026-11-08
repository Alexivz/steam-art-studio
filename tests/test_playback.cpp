#include "preview.h"
#include "platform.h"
#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

using namespace Studio;
namespace {
QString fixtureTemplate() {
#ifdef Q_OS_WIN
    const auto base = QDir::tempPath() + "/SteamArtStudio-playback-tests";
    QDir().mkpath(base); return base + "/fixture-XXXXXX";
#else
    return "/tmp/opencode/studio-playback-XXXXXX";
#endif
}
QByteArray read(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
bool animation(const QString &path, const QString &filter, const QString &format) {
    QProcess encoder;
    QStringList args = {"-v", "error", "-f", "lavfi", "-i", filter};
    if (format == "apng") args << "-plays" << "0";
    else args << "-loop" << "0";
    args << "-f" << format << "-y" << path;
    encoder.start(Platform::findTool("ffmpeg"), args);
    return encoder.waitForFinished(20000) && encoder.exitCode() == 0;
}
}
class PlaybackTests : public QObject {
    Q_OBJECT
private slots:
    void stillPreviewDoesNotUpscaleDecodedPixels() {
        QTemporaryDir temp(fixtureTemplate()); QVERIFY(temp.isValid());
        QImage image(32, 48, QImage::Format_ARGB32); image.fill(Qt::cyan);
        const auto file = temp.path() + "/still.png"; QVERIFY(image.save(file)); const auto original = read(file);
        Preview preview; preview.setFile(file, false); QCOMPARE(preview.decodedSize(), image.size());
        QCOMPARE(read(file), original);
    }
    void sixtyFpsAnimationUsesNativeCanvasAndPassthroughTiming() {
        if (Platform::findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg unavailable");
        QTemporaryDir temp(fixtureTemplate()); QVERIFY(temp.isValid()); const auto file = temp.path() + "/native.png";
        QVERIFY(animation(file, "testsrc2=size=600x900:rate=60:duration=0.2", "apng")); const auto original = read(file);
        Preview preview; preview.show(); preview.setFile(file, true);
        QSignalSpy frames(&preview, &Preview::frameReady); QSignalSpy errors(&preview, &Preview::error);
        preview.togglePlayback(); QTRY_VERIFY(frames.size() >= 3);
        QCOMPARE(preview.decodedSize(), QSize(600, 900));
        for (const auto &frame : frames) QCOMPARE(frame.first().toSize(), QSize(600, 900));
        auto *decoder = preview.findChild<QProcess *>("previewDecoder"); QVERIFY(decoder);
        const auto args = decoder->arguments(); const auto joined = args.join(' ');
        QVERIFY(!joined.contains("fps=")); QVERIFY(!joined.contains("scale="));
        QCOMPARE(args.value(args.indexOf("-fps_mode") + 1), QString("passthrough")); QVERIFY(args.contains("-re"));
        preview.stop(); QVERIFY(!preview.playing()); QCOMPARE(errors.size(), 0); QCOMPARE(read(file), original);
    }
    void smallGifRemainsNativeWhilePlayingAndStopsWhenHidden() {
        if (Platform::findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg unavailable");
        QTemporaryDir temp(fixtureTemplate()); QVERIFY(temp.isValid()); const auto file = temp.path() + "/small.gif";
        QVERIFY(animation(file, "testsrc=size=32x48:rate=4:duration=1", "gif"));
        Preview preview; preview.show(); preview.setFile(file, true); QCOMPARE(preview.decodedSize(), QSize(32, 48));
        QSignalSpy frames(&preview, &Preview::frameReady); preview.togglePlayback(); QTRY_VERIFY(!frames.isEmpty());
        QCOMPARE(preview.decodedSize(), QSize(32, 48)); preview.hide(); QVERIFY(!preview.playing());
    }
    void nativePlaybackKeepsTransparencyLikeTheOriginalStill() {
        if (Platform::findTool("ffmpeg").isEmpty()) QSKIP("FFmpeg unavailable");
        QTemporaryDir temp(fixtureTemplate()); QVERIFY(temp.isValid()); const auto file = temp.path() + "/alpha.png";
        QVERIFY(animation(file, "color=red:size=32x48:rate=60:duration=0.2,format=rgba,colorchannelmixer=aa=0.5", "apng"));
        Preview preview; preview.resize(320, 300); preview.setBackgroundMode(2); preview.show(); preview.setFile(file, true);
        const auto before = preview.grab().toImage().pixelColor(preview.width() / 2, preview.height() / 2);
        QVERIFY(before.green() > 0); QVERIFY(before.red() > before.green());
        QSignalSpy frames(&preview, &Preview::frameReady); preview.togglePlayback(); QTRY_VERIFY(!frames.isEmpty());
        const auto after = preview.grab().toImage().pixelColor(preview.width() / 2, preview.height() / 2);
        QCOMPARE(after, before); preview.stop();
    }
};
QTEST_MAIN(PlaybackTests)
#include "test_playback.moc"
