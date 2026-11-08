#include "platform.h"
#include "cache.h"
#include "draft.h"
#include "steam.h"
#include "transactions.h"
#include "transfers.h"
#include "fakenetwork.h"
#include "libraryindex.h"
#include "animation.h"
#include "animationfixtures.h"
#include <QtTest>
#include <QBuffer>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QProcess>

using namespace Studio;
namespace {
QString temporaryTemplate() {
#ifdef Q_OS_WIN
    const auto base = QDir::tempPath() + "/SteamArtStudio-tests"; QDir().mkpath(base); return base + "/portable-XXXXXX";
#else
    return "/tmp/opencode/studio-portable-XXXXXX";
#endif
}
bool write(const QString &path, const QByteArray &bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
QByteArray read(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
QByteArray png(const QColor &color) { QImage image(40, 60, QImage::Format_ARGB32); image.fill(color); QBuffer buffer; buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG"); return buffer.data(); }
Game gameAt(const QString &root) { Game game; game.appId = 100; game.name = "Synthetic Windows/Linux fixture"; game.steamRoot = root; game.userId = "42"; return game; }
}
class PortableTests : public QObject {
    Q_OBJECT
private slots:
    void slowOriginalDownloadAndStallRetryArePortable() {
        QTemporaryDir temp(temporaryTemplate()); QVERIFY(temp.isValid()); CacheStore cache(temp.path()); FakeNetwork transport;
        FakeResponse slow; slow.body = QByteArray(8192, 'a'); slow.chunk = 1024; slow.delay = 25; transport.responses.enqueue(slow);
        NetworkPolicy limits; limits.deadlineMs = 60; limits.originalDeadlineMs = 1500; limits.originalConnectionMs = limits.originalTransferMs = 1000;
        Network network(&cache, nullptr, &transport, limits); TransferModel tasks(&network); QSignalSpy complete(&tasks, &TransferModel::completed);
        const QUrl url("https://cdn2.steamgriddb.com/grid/slow-synthetic.webp");
        QVERIFY(tasks.enqueue(url, "Synthetic", {})); QTRY_COMPARE(complete.size(), 1); QCOMPARE(read(cache.lookup(url, 60).file), slow.body);
        FakeResponse stalled; stalled.body = QByteArray(8192, 'b'); stalled.chunk = 1024; stalled.stallAfterBytes = 1024; transport.responses.enqueue(stalled); transport.responses.enqueue(FakeResponse{});
        const QUrl other("https://cdn2.steamgriddb.com/grid/stalled-synthetic.webp");
        QVERIFY(tasks.enqueue(other, "Synthetic stall", {}));
        QTRY_COMPARE(tasks.index(1).data(TransferModel::StateRole).toInt(), int(TransferModel::Failed)); QVERIFY(!cache.lookup(other, 60).valid());
        tasks.retry(1); QTRY_COMPARE(complete.size(), 2); QVERIFY(cache.lookup(other, 60).valid()); QCOMPARE(transport.requests.size(), 3);
    }
    void animatedWebpCanConvertWithAvailableFfmpeg() {
        const auto ffmpeg = Platform::findTool("ffmpeg"); if (ffmpeg.isEmpty()) QSKIP("FFmpeg not bundled/installed");
        QTemporaryDir temp(temporaryTemplate()); QVERIFY(temp.isValid());
        const auto source = temp.path() + "/input.webp", destination = temp.path() + "/output.png";
        QVERIFY(TestAnimation::uniqueSixtyFpsWebp(source, QSize(32, 48), ffmpeg));
        QString error; QVERIFY2(convertAnimation(source, destination, "capsule", AnimationProfile::Original, &error), qPrintable(error));
        QCOMPARE(read(destination).count("fcTL"), 30);
        const auto before = TestAnimation::decodedRgba(source, ffmpeg), after = TestAnimation::decodedRgba(destination, ffmpeg); QVERIFY(!before.isEmpty());
        QCOMPARE(before.size(), 32 * 48 * 4 * 30); QCOMPARE(CacheStore::hash(after), CacheStore::hash(before));
    }
    void naturalOrderEvenWithCLocale() {
        const QLocale previous; QLocale::setDefault(QLocale::c());
        LibraryIndex index; Game a, b; a.appId = 10; a.name = "Game 10"; b.appId = 2; b.name = "Game 2";
        index.setGames({a, b}); const auto result = index.query({}); QLocale::setDefault(previous);
        QCOMPARE(result.first().appId, 2U);
    }
    void rootAndSiblingContainment() {
        QVERIFY(Platform::sameOrBelow("/cache", "/")); QVERIFY(Platform::sameOrBelow("/data/cache", "/data"));
        QVERIFY(!Platform::sameOrBelow("/database", "/data")); QVERIFY(!Platform::sameOrBelow("/cache", "/data"));
        QVERIFY(Platform::sameOrBelow("c:/APP/cache", "C:/app", Qt::CaseInsensitive));
        QVERIFY(!Platform::sameOrBelow("C:/app2/cache", "C:/app", Qt::CaseInsensitive));
        QVERIFY(Platform::sameOrBelow("C:/app", "C:/", Qt::CaseInsensitive));
    }
    void nativeDefaultsAreSeparated() {
        for (bool demo : {false, true}) QVERIFY(DraftStore::separateDirectories(Platform::dataDirectory(demo), Platform::cacheDirectory(demo)));
    }
    void windowsCandidatesArePathsNotCommandsAndDeduplicated() {
        const auto paths = Platform::windowsSteamCandidates({"D:\\Juegos\\Steam", "d:/juegos/steam", ""}, "C:/Program Files", "C:/Program Files (x86)");
        QCOMPARE(paths, QStringList({"D:/Juegos/Steam", "C:/Program Files (x86)/Steam", "C:/Program Files/Steam"}));
    }
    void unicodeAndSpacePathsApplyAndRestore() {
        QTemporaryDir temp(temporaryTemplate()); QVERIFY(temp.isValid()); const auto steam = temp.path() + "/Steam Library üñicode";
        const auto game = gameAt(steam); const auto source = temp.path() + "/new art.png"; const auto target = steam + "/userdata/42/config/grid/100p.png";
        const auto before = png(Qt::blue), after = png(Qt::cyan); QVERIFY(write(target, before)); QVERIFY(write(source, after));
        Transactions transactions(temp.path() + "/app data/backups"); const auto applied = transactions.apply({{game, "capsule", source, "Synthetic", false}});
        QVERIFY2(applied.ok, qPrintable(applied.error)); QVERIFY(read(target) != before); QCOMPARE(read(source), after);
        const auto restored = transactions.restore(applied.id); QVERIFY2(restored.ok, qPrintable(restored.error)); QCOMPARE(read(target), before);
    }
    void externalChangesBlockRestoration() {
        QTemporaryDir temp(temporaryTemplate()); const auto steam = temp.path() + "/Steam", source = temp.path() + "/source.png", target = steam + "/userdata/42/config/grid/100p.png";
        QVERIFY(write(target, png(Qt::blue))); QVERIFY(write(source, png(Qt::cyan))); Transactions transactions(temp.path() + "/backups");
        const auto applied = transactions.apply({{gameAt(steam), "capsule", source, "Synthetic", false}}); QVERIFY(applied.ok);
        const auto external = png(Qt::yellow); QVERIFY(write(target, external)); QVERIFY(!transactions.restore(applied.id).ok); QCOMPARE(read(target), external);
    }
    void draftRoundtripAndCorruptRecovery() {
        QTemporaryDir temp(temporaryTemplate()); const auto data = temp.path() + "/data", cacheRoot = temp.path() + "/cache"; QDir().mkpath(data); CacheStore cache(cacheRoot);
        const auto source = temp.path() + "/source.png"; QVERIFY(write(source, png(Qt::cyan))); const auto imported = cache.importFile(source); QVERIFY(!imported.isEmpty());
        DraftStore store(data, cacheRoot); const PendingChange change{gameAt(temp.path() + "/Steam"), "capsule", imported, "Synthetic", false};
        QVERIFY(store.save({change})); QCOMPARE(store.load().changes.size(), 1); QVERIFY(store.save({})); QVERIFY(write(data + "/draft.json", "broken"));
        QCOMPARE(store.load().changes.size(), 1); QCOMPARE(QDir(data).entryList({"draft.corrupt-*.json"}, QDir::Files).size(), 1);
    }
    void linksAndWindowsJunctionsAreNotWriteTargets() {
        QTemporaryDir temp(temporaryTemplate()); const auto outside = temp.path() + "/outside", root = temp.path() + "/cache"; QDir().mkpath(outside); CacheStore cache(root); QVERIFY(QDir().rmdir(root + "/data"));
#ifdef Q_OS_WIN
        QProcess command; command.start(qEnvironmentVariable("SystemRoot") + "/System32/cmd.exe", {"/C", "mklink", "/J", QDir::toNativeSeparators(root + "/data"), QDir::toNativeSeparators(outside)});
        QVERIFY(command.waitForFinished(10000)); QVERIFY2(command.exitCode() == 0, "Could not create the synthetic junction");
#else
        QVERIFY(QFile::link(outside, root + "/data"));
#endif
        QVERIFY(Platform::isLink(QFileInfo(root + "/data"))); QString error;
        QVERIFY(cache.store(QUrl("https://cdn2.steamgriddb.com/grid/synthetic.png"), png(Qt::red), "image/png", &error).isEmpty()); QVERIFY(!error.isEmpty()); QVERIFY(QDir(outside).isEmpty());
    }
    void cachePinsProtectOriginals() {
        QTemporaryDir temp(temporaryTemplate()); CacheStore cache(temp.path()); const QUrl url("https://cdn2.steamgriddb.com/grid/synthetic.png");
        const auto original = png(Qt::blue); const auto file = cache.store(url, original, "image/png"); QVERIFY(!file.isEmpty()); cache.pin(file);
        cache.store(url, png(Qt::red), "image/png"); cache.clearUnpinned(); QCOMPARE(read(file), original); cache.unpin(file); cache.clearUnpinned(); QVERIFY(!QFileInfo::exists(file));
    }
    void simulatedNetworkCancellationAndRetry() {
        QTemporaryDir temp(temporaryTemplate()); CacheStore cache(temp.path()); FakeNetwork transport; FakeResponse stalled; stalled.stall = true; transport.responses.enqueue(stalled); transport.responses.enqueue(FakeResponse{});
        Network network(&cache, nullptr, &transport); TransferModel transfers(&network); QSignalSpy complete(&transfers, &TransferModel::completed);
        QVERIFY(transfers.enqueue(QUrl("https://cdn2.steamgriddb.com/grid/synthetic.png"), "Synthetic", {})); transfers.cancel(0); QCOMPARE(transfers.activeCount(), 0);
        transfers.retry(0); QTRY_COMPARE(complete.size(), 1); QCOMPARE(transport.requests.size(), 2); QVERIFY(!transport.requests.first().hasRawHeader("Authorization"));
    }
    void insecureEndpointsAndUnexpectedToolsAreRejected() {
        QVERIFY(!Network::allowed(QUrl("http://cdn2.steamgriddb.com/file.png"), false)); QVERIFY(!Network::allowed(QUrl("https://outside.invalid/file.png"), false));
        QVERIFY(!Network::allowed(QUrl("https://user:password@www.steamgriddb.com/api/v2/x"), true)); QVERIFY(Platform::findTool("untrusted-metadata-command").isEmpty());
    }
};
QTEST_GUILESS_MAIN(PortableTests)
#include "test_portable.moc"
