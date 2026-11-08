#include "valvekv.h"
#include "steam.h"
#include "cache.h"
#include "network.h"
#include "transactions.h"
#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QProcess>
#include <QStandardPaths>
#include <QUrlQuery>
#include "imageutils.h"
#include "draft.h"
#include "libraryindex.h"
#include "diagnostics.h"
#include "animationfixtures.h"
#include <QtEndian>

using namespace Studio;
namespace {
void write(const QString &path, const QByteArray &data) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), data.size()); }
QByteArray read(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
void u32(QByteArray &data, quint32 n) { uchar bytes[4]; qToLittleEndian(n, bytes); data.append(reinterpret_cast<char *>(bytes), 4); }
void u64(QByteArray &data, quint64 n) { uchar bytes[8]; qToLittleEndian(n, bytes); data.append(reinterpret_cast<char *>(bytes), 8); }
void field(QByteArray &data, quint8 type, quint32 key) { data.append(static_cast<char>(type)); u32(data, key); }
QByteArray metadata(const QString &iconHash = {}) {
    const QStringList keys{"appinfo", "common", "name", "type", "clienticon"}; QByteArray data; u32(data, 0x07564429); u32(data, 1); u64(data, 0);
    for (quint32 id : {100U, 200U}) {
        QByteArray entry; field(entry, 0, 0); field(entry, 0, 1); field(entry, 1, 2); entry += id == 100 ? "First Game" : "New Game"; entry += '\0';
        field(entry, 1, 3); entry += "Game"; entry += '\0';
        if (id == 100 && !iconHash.isEmpty()) { field(entry, 1, 4); entry += iconHash.toUtf8(); entry += '\0'; }
        entry += QByteArray(3, '\x08');
        u32(data, id); u32(data, 60 + entry.size()); data += QByteArray(60, '\0'); data += entry;
    }
    u32(data, 0); const auto offset = static_cast<quint64>(data.size()); qToLittleEndian(offset, reinterpret_cast<uchar *>(data.data() + 8));
    u32(data, keys.size()); for (const auto &key : keys) { data += key.toUtf8(); data += '\0'; } return data;
}
QByteArray local(bool added = false) {
    return QByteArray("\"UserLocalConfigStore\" { \"Software\" { \"Valve\" { \"Steam\" { \"apps\" { \"100\" { \"LastPlayed\" \"1000\" } ")
        + (added ? "\"200\" { \"LastPlayed\" \"2000\" } " : "") + "} } } } }";
}
void fixture(const QString &root) {
    QDir().mkpath(root + "/appcache/librarycache"); QDir().mkpath(root + "/userdata/42/config/grid");
    write(root + "/config/loginusers.vdf", "\"users\" { \"76561197960265770\" { \"MostRecent\" \"1\" } }");
    write(root + "/appcache/appinfo.vdf", metadata()); write(root + "/userdata/42/config/localconfig.vdf", local());
    write(root + "/steamapps/libraryfolders.vdf", "\"libraryfolders\" { \"0\" { \"apps\" { \"100\" \"42\" } } }");
    write(root + "/steamapps/appmanifest_100.acf", "\"AppState\" { \"appid\" \"100\" \"name\" \"First Game\" }");
}
QByteArray png(const QColor &color, const char *format = "PNG") { QImage image(30, 45, QImage::Format_RGB32); image.fill(color); QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer, format); return bytes; }
PendingChange change(const QString &root, const QString &image) { Game game; game.appId = 100; game.name = "First Game"; game.steamRoot = root; game.userId = "42"; return {game, "capsule", image, "Local test", false}; }
struct ApngInfo { quint32 frames = 0; double seconds = 0; QSize size; };
ApngInfo apngInfo(const QByteArray &bytes) {
    ApngInfo result; qsizetype offset = 8;
    while (offset + 12 <= bytes.size()) {
        const auto size = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
        if (size > bytes.size() - offset - 12) break;
        const auto kind = bytes.mid(offset + 4, 4); const auto *data = reinterpret_cast<const uchar *>(bytes.constData() + offset + 8);
        if (kind == "IHDR" && size >= 8) result.size = QSize(qFromBigEndian<quint32>(data), qFromBigEndian<quint32>(data + 4));
        if (kind == "acTL" && size >= 8) result.frames = qFromBigEndian<quint32>(data);
        if (kind == "fcTL" && size >= 26) { const auto denominator = qFromBigEndian<quint16>(data + 22); result.seconds += qFromBigEndian<quint16>(data + 20) / double(denominator ? denominator : 100); }
        offset += size + 12;
    }
    return result;
}
bool syntheticWebp(const QString &path, const QString &video) {
    QProcess process; process.start(QStandardPaths::findExecutable("ffmpeg"), {"-v", "error", "-f", "lavfi", "-i", video, "-c:v", "libwebp_anim", "-loop", "0", "-y", path});
    return process.waitForFinished(20000) && process.exitCode() == 0;
}
QByteArray decodedRgba(const QString &path) {
    return TestAnimation::decodedRgba(path, QStandardPaths::findExecutable("ffmpeg"));
}
void key(QByteArray &data, quint8 type, const QByteArray &name) { data.append(static_cast<char>(type)); data += name; data += '\0'; }
void stringField(QByteArray &data, const QByteArray &name, const QByteArray &value) { key(data, 1, name); data += value; data += '\0'; }
QByteArray shortcuts(const QVector<quint32> &ids, bool withIcons = true) {
    QByteArray data; key(data, 0, "shortcuts");
    for (int i = 0; i < ids.size(); ++i) {
        key(data, 0, QByteArray::number(i)); key(data, 2, "appid"); u32(data, ids.at(i));
        stringField(data, "AppName", "External " + QByteArray::number(i));
        stringField(data, "Exe", "\"/synthetic/never-execute\"");
        stringField(data, "LaunchOptions", "synthetic-option");
        if (withIcons) stringField(data, "icon", "/old/" + QByteArray::number(i) + ".png");
        key(data, 0, "tags"); stringField(data, "0", "Keep exactly"); data += '\x08';
        // This unknown key and non-UTF-8 byte must not be re-serialized.
        stringField(data, "future_field", QByteArray("raw-\xff", 5));
        key(data, 5, "future_wide"); data += QByteArray::fromHex("0300410042000000"); data += '\x08';
    }
    data += QByteArray(2, '\x08'); return data;
}
PendingChange iconChange(const QString &root, const QString &source, bool external = false, quint32 id = 100) {
    auto item = change(root, source); item.type = "icon"; item.game.external = external; item.game.appId = id;
    if (!external) item.game.iconHash = QString(40, 'a');
    return item;
}
QByteArray multiResolutionIco() {
    QVector<QByteArray> images;
    for (int size : {16, 256}) { QImage image(size, size, QImage::Format_ARGB32); image.fill(size == 16 ? Qt::red : Qt::blue); QByteArray data; QBuffer buffer(&data); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG"); images.append(data); }
    QByteArray ico = QByteArray::fromHex("000001000200"); quint32 offset = 38;
    for (int i = 0; i < 2; ++i) { const char size = i == 0 ? 16 : 0; ico.append(size); ico.append(size); ico += QByteArray::fromHex("000001002000"); u32(ico, images.at(i).size()); u32(ico, offset); offset += images.at(i).size(); }
    for (const auto &image : images) ico += image;
    return ico;
}
}
class StudioTests : public QObject {
    Q_OBJECT
private slots:
    void textParser() {
        QString error; const auto data = ValveKv::text("// comment\n\"Root\" { \"name\" \"A \\\"quoted\\\" name\" \"path\" \"C:\\\\games\" }", &error);
        QVERIFY(error.isEmpty()); const auto root = ValveKv::child(data, "root"); QCOMPARE(ValveKv::value(root, "NAME").toString(), "A \"quoted\" name"); QCOMPARE(root.value("path").toString(), "C:\\games");
    }
    void malformedText() { QString error; QVERIFY(ValveKv::text("\"root\" { \"broken", &error).isEmpty()); QVERIFY(!error.isEmpty()); }
    void modernAppInfo() { QString error; auto data = ValveKv::appInfo(metadata(), &error); QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(data.size(), 2); QCOMPARE(ValveKv::value(ValveKv::child(data.value(200), "common"), "name").toString(), "New Game"); }
    void truncatedAppInfo() { QString error; QVERIFY(ValveKv::appInfo(metadata().left(30), &error).isEmpty()); QVERIFY(!error.isEmpty()); }
    void badTable() { auto data = metadata(); qToLittleEndian<quint64>(0xffffffffffffffffULL, reinterpret_cast<uchar *>(data.data() + 8)); QString error; QVERIFY(ValveKv::appInfo(data, &error).isEmpty()); QVERIFY(!error.isEmpty()); }
    void binaryShortcut() {
        QByteArray data; data.append('\0'); data += "shortcuts"; data += '\0'; data.append('\0'); data += "0"; data += '\0';
        data.append('\x02'); data += "appid"; data += '\0'; u32(data, 0x80000042); data.append('\x01'); data += "AppName"; data += '\0'; data += "External Game"; data += '\0'; data += QByteArray(3, '\x08');
        QString error; auto root = ValveKv::binary(data, {}, &error); QVERIFY(error.isEmpty()); auto shortcut = ValveKv::child(ValveKv::child(root, "shortcuts"), "0"); QCOMPARE(shortcut.value("appid").toUInt(), 0x80000042U);
    }
    void localDiscoveryWithoutImages() {
        QTemporaryDir dir; fixture(dir.path()); auto before = read(dir.path() + "/userdata/42/config/localconfig.vdf");
        auto scan = SteamScanner::scan(dir.path()); QVERIFY(scan.error.isEmpty()); QCOMPARE(scan.games.size(), 1); QVERIFY(scan.games.first().installed); QVERIFY(scan.games.first().cover.isEmpty()); QCOMPARE(read(dir.path() + "/userdata/42/config/localconfig.vdf"), before);
    }
    void liveMetadataAddition() { QTemporaryDir dir; fixture(dir.path()); QCOMPARE(SteamScanner::scan(dir.path()).games.size(), 1); write(dir.path() + "/userdata/42/config/localconfig.vdf", local(true)); auto scan = SteamScanner::scan(dir.path()); QCOMPARE(scan.games.size(), 2); }
    void installationDeduplication() {
        QTemporaryDir home; const auto root = home.path() + "/.local/share/Steam"; fixture(root); QDir().mkpath(home.path() + "/.steam"); QVERIFY(QFile::link(root, home.path() + "/.steam/steam")); QCOMPARE(SteamScanner::discover(home.path()).size(), 1);
    }
    void accountTraversalRejected() { QTemporaryDir dir; fixture(dir.path()); QVERIFY(!SteamScanner::scan(dir.path(), "../../outside").error.isEmpty()); Game game; game.steamRoot = dir.path(); game.userId = "../x"; QVERIFY(SteamScanner::gridDirectory(game).isEmpty()); }
    void networkScope_data() { QTest::addColumn<QString>("url"); QTest::addColumn<bool>("api"); QTest::addColumn<bool>("accepted");
        QTest::newRow("api") << "https://www.steamgriddb.com/api/v2/games/steam/100" << true << true;
        QTest::newRow("cdn") << "https://cdn2.steamgriddb.com/grid/test.png" << false << true;
        QTest::newRow("http") << "http://www.steamgriddb.com/api/v2/" << true << false;
        QTest::newRow("evil-suffix") << "https://www.steamgriddb.com.evil.test/api/v2/" << true << false;
        QTest::newRow("userinfo") << "https://user:password@www.steamgriddb.com/api/v2/" << true << false;
        QTest::newRow("cdn-api") << "https://cdn2.steamgriddb.com/api/v2/" << true << false;
        QTest::newRow("file") << "file:///etc/passwd" << false << false;
        QTest::newRow("port") << "https://www.steamgriddb.com:444/api/v2/" << true << false;
    }
    void networkScope() { QFETCH(QString, url); QFETCH(bool, api); QFETCH(bool, accepted); QCOMPARE(Network::allowed(QUrl(url), api), accepted); }
    void cacheFilterIsolation() {
        QTemporaryDir dir; CacheStore cache(dir.path()); const QUrl safe("https://www.steamgriddb.com/api/v2/grids/game/100?nsfw=false"), adult("https://www.steamgriddb.com/api/v2/grids/game/100?nsfw=any");
        QString error; auto first = cache.store(safe, "public-safe", "application/json", &error); QVERIFY(!first.isEmpty()); QVERIFY(cache.lookup(safe, 60).fresh); QVERIFY(!cache.lookup(adult, 60).valid());
        auto second = cache.store(adult, "public-adult", "application/json"); QVERIFY(first != second); QCOMPARE(read(first), QByteArray("public-safe"));
    }
    void cachePersistence() { QTemporaryDir dir; QUrl url("https://cdn2.steamgriddb.com/grid/a.png"); { CacheStore cache(dir.path()); QVERIFY(!cache.store(url, "data", "image/png").isEmpty()); } CacheStore reopened(dir.path()); QVERIFY(reopened.lookup(url, 60).valid()); }
    void corruptedCache() { QTemporaryDir dir; CacheStore cache(dir.path()); QUrl url("https://cdn2.steamgriddb.com/grid/a.png"); cache.store(url, "data", "image/png"); write(dir.path() + "/metadata/" + CacheStore::hash(url.toEncoded(QUrl::FullyEncoded)) + ".json", "broken"); QVERIFY(!cache.lookup(url, 60).valid()); }
    void pinnedCacheSurvivesEviction() { QTemporaryDir dir; CacheStore cache(dir.path(), 9000); auto a = cache.store(QUrl("https://cdn2.steamgriddb.com/a"), QByteArray(6000, 'a'), "image/png"); cache.pin(a); auto b = cache.store(QUrl("https://cdn2.steamgriddb.com/b"), QByteArray(6000, 'b'), "image/png"); cache.prune(); QVERIFY(QFileInfo::exists(a)); QVERIFY(!QFileInfo::exists(b)); cache.clearUnpinned(); QVERIFY(QFileInfo::exists(a)); }
    void sharedPinsAreReferenceCounted() { QTemporaryDir dir; CacheStore cache(dir.path()); auto path = cache.store(QUrl("https://cdn2.steamgriddb.com/shared"), "data", "image/png"); cache.pin(path); cache.pin(path); cache.unpin(path); cache.clearUnpinned(); QVERIFY(QFileInfo::exists(path)); cache.unpin(path); cache.clearUnpinned(); QVERIFY(!QFileInfo::exists(path)); }
    void applyAndRestore() {
        QTemporaryDir dir; const auto steam = dir.path() + "/steam"; fixture(steam); const auto grid = steam + "/userdata/42/config/grid/100p";
        const auto original = png(Qt::blue, "JPEG"); write(grid + ".jpg", original); const auto source = dir.path() + "/new.png"; write(source, png(Qt::red));
        Transactions transactions(dir.path() + "/backups"); const auto applied = transactions.apply({change(steam, source)}); QVERIFY2(applied.ok, qPrintable(applied.error)); QVERIFY(QFileInfo::exists(grid + ".png")); QVERIFY(!QFileInfo::exists(grid + ".jpg"));
        QImage image(grid + ".png"); QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red)); const auto restored = transactions.restore(applied.id); QVERIFY2(restored.ok, qPrintable(restored.error)); QCOMPARE(read(grid + ".jpg"), original); QVERIFY(!QFileInfo::exists(grid + ".png")); QVERIFY(!transactions.restore(applied.id).ok);
    }
    void externalChangesBlockRestore() {
        QTemporaryDir dir; const auto steam = dir.path() + "/steam"; fixture(steam); const auto source = dir.path() + "/new.png"; write(source, png(Qt::red)); Transactions transactions(dir.path() + "/backups"); auto applied = transactions.apply({change(steam, source)}); QVERIFY(applied.ok);
        const auto target = steam + "/userdata/42/config/grid/100p.png"; const auto different = png(Qt::green); write(target, different); auto restored = transactions.restore(applied.id); QVERIFY(!restored.ok); QCOMPARE(read(target), different);
    }
    void interruptedRestoreCanResume() {
        QTemporaryDir dir; const auto steam = dir.path() + "/steam"; fixture(steam);
        const auto source = dir.path() + "/new.png"; write(source, png(Qt::red));
        Transactions transactions(dir.path() + "/backups"); const auto applied = transactions.apply({change(steam, source)}); QVERIFY(applied.ok);
        const auto manifestPath = transactions.root() + "/" + applied.id + "/manifest.json";
        auto manifest = QJsonDocument::fromJson(read(manifestPath)).object(); manifest["status"] = "restoring";
        write(manifestPath, QJsonDocument(manifest).toJson());
        QVERIFY(QFile::remove(steam + "/userdata/42/config/grid/100p.png"));
        QVERIFY(transactions.restore(applied.id).ok);
    }
    void invalidBatchDoesNotPartiallyWrite() {
        QTemporaryDir dir; const auto steam = dir.path() + "/steam"; fixture(steam); const auto source = dir.path() + "/new.png"; write(source, png(Qt::red)); auto a = change(steam, source), b = a; b.game.appId = 200; b.game.userId = "../outside";
        Transactions transactions(dir.path() + "/backups"); QVERIFY(!transactions.apply({a, b}).ok); QVERIFY(!QFileInfo::exists(steam + "/userdata/42/config/grid/100p.png"));
    }
    void symlinkGridRejected() {
        QTemporaryDir dir; const auto steam = dir.path() + "/steam"; fixture(steam); const auto grid = steam + "/userdata/42/config/grid"; QVERIFY(QDir().rmdir(grid)); QDir().mkpath(dir.path() + "/outside"); QVERIFY(QFile::link(dir.path() + "/outside", grid)); const auto source = dir.path() + "/image.png"; write(source, png(Qt::red)); Transactions transactions(dir.path() + "/backups"); QVERIFY(!transactions.apply({change(steam, source)}).ok); QVERIFY(QDir(dir.path() + "/outside").entryList(QDir::Files).isEmpty());
    }
    void unknownTypeRejected() { QTemporaryDir dir; fixture(dir.path() + "/steam"); const auto source = dir.path() + "/image.png"; write(source, png(Qt::red)); auto item = change(dir.path() + "/steam", source); item.type = "../escape"; Transactions transactions(dir.path() + "/backups"); QVERIFY(!transactions.apply({item}).ok); }
    void invalidImageRejected() { QTemporaryDir dir; auto file = dir.path() + "/fake.png"; write(file, "<svg></svg>"); QString error; QVERIFY(!Transactions::validImage(file, nullptr, &error)); QVERIFY(!error.isEmpty()); }
    void corruptedBackupRejected() {
        QTemporaryDir dir; const auto steam = dir.path() + "/steam"; fixture(steam); write(steam + "/userdata/42/config/grid/100p.png", png(Qt::blue)); const auto source = dir.path() + "/new.png"; write(source, png(Qt::red)); Transactions transactions(dir.path() + "/backups"); const auto applied = transactions.apply({change(steam, source)}); QVERIFY(applied.ok); write(transactions.root() + "/" + applied.id + "/original-0.bin", "corrupted"); QVERIFY(!transactions.restore(applied.id).ok);
    }
    void animatedGifConvertsToApng() {
        const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg"); if (ffmpeg.isEmpty()) QSKIP("FFmpeg no está instalado");
        QTemporaryDir dir; const auto steam = dir.path() + "/steam"; fixture(steam); const auto gif = dir.path() + "/animation.gif";
        QProcess process; process.start(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "testsrc=size=32x48:rate=4:duration=1", "-loop", "0", "-y", gif});
        QVERIFY(process.waitForFinished(10000)); QCOMPARE(process.exitCode(), 0);
        bool animated = false; QString error; QVERIFY2(Transactions::validImage(gif, &animated, &error), qPrintable(error)); QVERIFY(animated);
        QVERIFY(!Transactions::validIcon(gif, &error));
        auto item = change(steam, gif); item.animated = true; Transactions transactions(dir.path() + "/backups");
        auto applied = transactions.apply({item}); QVERIFY2(applied.ok, qPrintable(applied.error));
        const auto bytes = read(steam + "/userdata/42/config/grid/100p.png"); QVERIFY(bytes.startsWith(QByteArray::fromHex("89504e47"))); QVERIFY(bytes.contains("acTL")); QVERIFY(transactions.restore(applied.id).ok);
    }
    void animatedWebpPreserves494FramesAndDurationByDefault() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir dir("/tmp/opencode/studio-animation-XXXXXX"); QVERIFY(dir.isValid()); const auto source = dir.path() + "/synthetic.webp";
        QVERIFY(syntheticWebp(source, "testsrc2=size=96x144:rate=50:duration=9.88")); const auto original = read(source);
        fixture(dir.path() + "/steam"); Transactions transactions(dir.path() + "/backups"); auto item = change(dir.path() + "/steam", source); item.animated = true;
        const auto applied = transactions.apply({item}); QVERIFY2(applied.ok, qPrintable(applied.error));
        const auto info = apngInfo(read(dir.path() + "/steam/userdata/42/config/grid/100p.png"));
        QCOMPARE(info.frames, 494U); QCOMPARE(info.size, QSize(96, 144)); QVERIFY(qAbs(info.seconds - 9.88) < 0.03); QCOMPARE(read(source), original);
        QVERIFY(transactions.restore(applied.id).ok);
    }
    void originalSixtyFpsWebpPreservesDecodedPixelsAlphaAndTiming() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir dir("/tmp/opencode/studio-animation-XXXXXX"); QVERIFY(dir.isValid()); const auto source = dir.path() + "/source.webp";
        QVERIFY(TestAnimation::uniqueSixtyFpsWebp(source, QSize(64, 96), QStandardPaths::findExecutable("ffmpeg")));
        const auto original = read(source), sourcePixels = decodedRgba(source); QVERIFY(!sourcePixels.isEmpty());
        fixture(dir.path() + "/steam"); Transactions transactions(dir.path() + "/backups"); auto item = change(dir.path() + "/steam", source); item.animated = true;
        const auto applied = transactions.apply({item}); QVERIFY2(applied.ok, qPrintable(applied.error));
        const auto target = dir.path() + "/steam/userdata/42/config/grid/100p.png"; const auto info = apngInfo(read(target));
        QCOMPARE(info.frames, 30U); QCOMPARE(info.size, QSize(64, 96)); QVERIFY(qAbs(info.seconds - 0.5) < 0.03);
        const auto outputPixels = decodedRgba(target); QCOMPARE(outputPixels.size(), sourcePixels.size()); QCOMPARE(CacheStore::hash(outputPixels), CacheStore::hash(sourcePixels));
        QVERIFY(QImage(target).pixelColor(0, 0).alpha() < 255); QCOMPARE(read(source), original); QVERIFY(transactions.restore(applied.id).ok);
    }
    void originalNativeApngIsAppliedByteForByte() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir dir("/tmp/opencode/studio-animation-XXXXXX"); const auto source = dir.path() + "/source.png";
        QProcess encoder; encoder.start(QStandardPaths::findExecutable("ffmpeg"), {"-v", "error", "-f", "lavfi", "-i", "testsrc2=size=64x96:rate=60:duration=0.5", "-plays", "0", "-f", "apng", "-y", source});
        QVERIFY(encoder.waitForFinished(10000)); QCOMPARE(encoder.exitCode(), 0); const auto original = read(source);
        fixture(dir.path() + "/steam"); const auto target = dir.path() + "/steam/userdata/42/config/grid/100p.png"; const auto previous = png(Qt::blue); write(target, previous);
        Transactions transactions(dir.path() + "/backups"); auto item = change(dir.path() + "/steam", source); item.animated = true;
        const auto applied = transactions.apply({item}); QVERIFY2(applied.ok, qPrintable(applied.error));
        QCOMPARE(read(target), original); QCOMPARE(read(source), original); QVERIFY(transactions.restore(applied.id).ok); QCOMPARE(read(target), previous);
    }
    void animationOptimizationIsExplicitAndPreservesAspectAndDuration() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir dir("/tmp/opencode/studio-animation-XXXXXX"); QVERIFY(dir.isValid()); const auto source = dir.path() + "/synthetic.webp";
        QVERIFY(syntheticWebp(source, "testsrc2=size=720x1080:rate=50:duration=0.5")); const auto original = read(source); QString error;
        QVERIFY2(convertAnimation(source, dir.path() + "/balanced.png", "capsule", AnimationProfile::Balanced, &error), qPrintable(error));
        const auto balanced = apngInfo(read(dir.path() + "/balanced.png")); QCOMPARE(balanced.size, QSize(600, 900)); QVERIFY(balanced.frames > 1 && balanced.frames <= 11); QVERIFY(qAbs(balanced.seconds - 0.5) < 0.1);
        QVERIFY2(convertAnimation(source, dir.path() + "/small.png", "capsule", AnimationProfile::Small, &error), qPrintable(error));
        const auto small = apngInfo(read(dir.path() + "/small.png")); QCOMPARE(small.size, QSize(400, 600)); QVERIFY(small.frames > 1 && small.frames <= 7); QVERIFY(qAbs(small.seconds - 0.5) < 0.1); QCOMPARE(read(source), original);
        fixture(dir.path() + "/steam"); const auto target = dir.path() + "/steam/userdata/42/config/grid/100p.png"; const auto previous = png(Qt::blue); write(target, previous);
        Transactions transactions(dir.path() + "/backups"); auto item = change(dir.path() + "/steam", source); item.animated = true;
        const auto applied = transactions.apply({item}, AnimationProfile::Small); QVERIFY2(applied.ok, qPrintable(applied.error)); QCOMPARE(apngInfo(read(target)).size, QSize(400, 600));
        QCOMPARE(read(source), original); QVERIFY(transactions.restore(applied.id).ok); QCOMPARE(read(target), previous);
    }
    void animationOptimizationDoesNotDuplicateSlowFramesOrUpscale() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir dir("/tmp/opencode/studio-animation-XXXXXX"); const auto source = dir.path() + "/synthetic.webp";
        QVERIFY(syntheticWebp(source, "testsrc=size=32x48:rate=4:duration=1,format=rgba,colorchannelmixer=aa=0.5")); QString error;
        QVERIFY2(convertAnimation(source, dir.path() + "/small.png", "capsule", AnimationProfile::Small, &error), qPrintable(error));
        const auto info = apngInfo(read(dir.path() + "/small.png")); QCOMPARE(info.frames, 4U); QCOMPARE(info.size, QSize(32, 48)); QVERIFY(qAbs(info.seconds - 1) < 0.03);
        const QImage firstFrame(dir.path() + "/small.png"); QVERIFY(!firstFrame.isNull()); QVERIFY(firstFrame.hasAlphaChannel()); QVERIFY(firstFrame.pixelColor(0, 0).alpha() < 255);
    }
    void animationSizeAndTimeFailuresAreDistinctAndRemovePartialOutput() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir dir("/tmp/opencode/studio-animation-XXXXXX"); const auto source = dir.path() + "/synthetic.webp";
        QVERIFY(syntheticWebp(source, "testsrc2=size=320x480:rate=50:duration=2")); const auto original = read(source); QString error;
        AnimationLimits size; size.maximumBytes = 100;
        QVERIFY(!convertAnimation(source, dir.path() + "/large.png", "capsule", AnimationProfile::Original, &error, size)); QVERIFY(error.contains("APNG convertido supera")); QVERIFY(!QFileInfo::exists(dir.path() + "/large.png"));
        AnimationLimits time; time.timeoutMs = 1;
        QVERIFY(!convertAnimation(source, dir.path() + "/slow.png", "capsule", AnimationProfile::Original, &error, time)); QVERIFY(error.contains("tiempo máximo")); QVERIFY(!QFileInfo::exists(dir.path() + "/slow.png")); QCOMPARE(read(source), original);
    }
    void animationConverterProtectsExistingDestinationsAndReportsInvalidInput() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir dir("/tmp/opencode/studio-animation-XXXXXX"); write(dir.path() + "/invalid.webp", "not a media file"); write(dir.path() + "/existing.png", "user work"); QString error;
        QVERIFY(!convertAnimation(dir.path() + "/invalid.webp", dir.path() + "/existing.png", "capsule", AnimationProfile::Original, &error)); QCOMPARE(read(dir.path() + "/existing.png"), QByteArray("user work"));
        QVERIFY(!convertAnimation(dir.path() + "/invalid.webp", dir.path() + "/new.png", "capsule", AnimationProfile::Original, &error)); QVERIFY(error.contains("FFmpeg no pudo")); QVERIFY(!error.contains(dir.path())); QVERIFY(!QFileInfo::exists(dir.path() + "/new.png"));
    }
    void iconApiUsesDedicatedEndpoint() {
        const auto url = ArtService::artworkUrl("icon", 77, true, true, false, 2);
        QCOMPARE(url.path(), "/api/v2/icons/game/77"); const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue("mimes"), "image/png,image/vnd.microsoft.icon");
        QCOMPARE(query.queryItemValue("types"), "static"); QCOMPARE(query.queryItemValue("page"), "2");
        QVERIFY(ArtService::artworkUrl("../escape", 77, true, false, false, 0).isEmpty());
        QVERIFY(url != ArtService::artworkUrl("capsule", 77, true, true, false, 2));
    }
    void shortcutPatchPreservesUnrelatedBytes() {
        const auto original = shortcuts({0x80000042U, 0x80000043U}); QString error;
        const auto updated = ValveKv::shortcutIcons(original, {{0x80000042U, "/new/icon.png"}}, &error);
        QVERIFY2(!updated.isEmpty(), qPrintable(error));
        auto expected = original; expected.replace("/old/0.png", "/new/icon.png"); QCOMPARE(updated, expected);
        QVERIFY(updated.contains(QByteArray("raw-\xff", 5)));
    }
    void shortcutPatchInsertsMissingField() {
        const auto original = shortcuts({0x80000042U}, false); QString error;
        const auto updated = ValveKv::shortcutIcons(original, {{0x80000042U, "/new/icon.png"}}, &error);
        QVERIFY2(!updated.isEmpty(), qPrintable(error));
        const auto item = ValveKv::child(ValveKv::child(ValveKv::binary(updated), "shortcuts"), "0");
        QCOMPARE(ValveKv::value(item, "icon").toString(), "/new/icon.png");
        auto unpatched = updated; QByteArray field; stringField(field, "icon", "/new/icon.png"); unpatched.replace(field, ""); QCOMPARE(unpatched, original);
    }
    void shortcutPatchRejectsAmbiguity() {
        const auto original = shortcuts({0x80000042U}); QString error;
        QVERIFY(ValveKv::shortcutIcons(shortcuts({0x80000042U, 0x80000042U}), {{0x80000042U, "/new.png"}}, &error).isEmpty());
        QVERIFY(ValveKv::shortcutIcons(original, {{0x80000099U, "/new.png"}}, &error).isEmpty());
        QVERIFY(ValveKv::shortcutIcons(original.chopped(1), {{0x80000042U, "/new.png"}}, &error).isEmpty());
        QVERIFY(ValveKv::shortcutIcons(original + '\0', {{0x80000042U, "/new.png"}}, &error).isEmpty());
        auto duplicate = original; QByteArray field; stringField(field, "icon", "/other.png"); duplicate.insert(duplicate.indexOf(QByteArray("\x01" "icon\0", 6)), field);
        QVERIFY(ValveKv::shortcutIcons(duplicate, {{0x80000042U, "/new.png"}}, &error).isEmpty());
    }
    void nativeIconDetectedReadOnly() {
        QTemporaryDir dir; fixture(dir.path()); const QString hash(40, 'a'); write(dir.path() + "/appcache/appinfo.vdf", metadata(hash));
        const QString path = dir.path() + "/appcache/librarycache/100/" + hash + ".jpg"; const auto original = png(Qt::blue); write(path, original);
        const auto scan = SteamScanner::scan(dir.path()); QCOMPARE(scan.games.size(), 1); QCOMPARE(scan.games.first().iconHash, hash); QCOMPARE(scan.games.first().icon, path); QCOMPARE(read(path), original);
    }
    void nativeIconApplyAndRestore() {
        QTemporaryDir dir; const auto steam = dir.path() + "/steam"; fixture(steam); const QString hash(40, 'a'); write(steam + "/appcache/appinfo.vdf", metadata(hash));
        const auto target = steam + "/appcache/librarycache/100/" + hash + ".jpg"; const auto original = png(Qt::blue); write(target, original);
        const auto source = dir.path() + "/icon.png"; write(source, png(Qt::red)); const auto item = iconChange(steam, source);
        QCOMPARE(Transactions::destination(item.game, "icon"), target);
        Transactions transactions(dir.path() + "/backups"); const auto applied = transactions.apply({item}); QVERIFY2(applied.ok, qPrintable(applied.error));
        QImage image(target); QCOMPARE(image.size(), QSize(256, 256)); QCOMPARE(image.pixelColor(128, 128), QColor(Qt::red)); QVERIFY(image.pixelColor(0, 0).alpha() == 0);
        QVERIFY(!QFileInfo::exists(steam + "/userdata/42/config/grid/100_icon.png"));
        QVERIFY(transactions.restore(applied.id).ok); QCOMPARE(read(target), original);
    }
    void legacyNativeIconTarget() {
        QTemporaryDir dir; fixture(dir.path()); write(dir.path() + "/appcache/appinfo.vdf", metadata(QString(40, 'a')));
        const QString target = dir.path() + "/appcache/librarycache/100_icon.jpg"; const auto original = png(Qt::blue); write(target, original);
        const QString source = dir.path() + "/source.png"; write(source, png(Qt::red)); auto item = iconChange(dir.path(), source);
        QCOMPARE(Transactions::destination(item.game, "icon"), target); Transactions tx(dir.path() + "/backups"); const auto result = tx.apply({item}); QVERIFY2(result.ok, qPrintable(result.error)); QVERIFY(tx.restore(result.id).ok); QCOMPARE(read(target), original);
    }
    void changedNativeIconHashCancelsWholeBatch() {
        QTemporaryDir dir; fixture(dir.path()); write(dir.path() + "/appcache/appinfo.vdf", metadata(QString(40, 'b'))); const auto source = dir.path() + "/source.png"; write(source, png(Qt::red));
        Transactions tx(dir.path() + "/backups"); const auto result = tx.apply({change(dir.path(), source), iconChange(dir.path(), source)}); QVERIFY(!result.ok);
        QVERIFY(!QFileInfo::exists(dir.path() + "/userdata/42/config/grid/100p.png"));
    }
    void iconCacheSymlinkRejected() {
        QTemporaryDir dir; fixture(dir.path() + "/steam"); QDir().mkpath(dir.path() + "/outside");
        QVERIFY(QFile::link(dir.path() + "/outside", dir.path() + "/steam/appcache/librarycache/100"));
        auto item = iconChange(dir.path() + "/steam", {}); QVERIFY(Transactions::destination(item.game, "icon").isEmpty());
        QJsonObject record{{"scope", "shortcuts"}, {"root", dir.path() + "/steam"}, {"user", "42"}, {"name", "localconfig.vdf"}}; QVERIFY(Transactions::recordDestination(record).isEmpty());
    }
    void shortcutIconBatchApplyAndRestore() {
        QTemporaryDir dir; fixture(dir.path()); const auto original = shortcuts({0x80000042U, 0x80000043U}); const auto config = dir.path() + "/userdata/42/config/shortcuts.vdf"; write(config, original);
        const auto source = dir.path() + "/source.png"; write(source, png(Qt::red));
        const auto a = iconChange(dir.path(), source, true, 0x80000042U), b = iconChange(dir.path(), source, true, 0x80000043U);
        Transactions tx(dir.path() + "/backups"); const auto result = tx.apply({a, b}); QVERIFY2(result.ok, qPrintable(result.error));
        const auto expected = ValveKv::shortcutIcons(original, {{a.game.appId, Transactions::destination(a.game, "icon")}, {b.game.appId, Transactions::destination(b.game, "icon")}}); QCOMPARE(read(config), expected);
        const auto manifest = QJsonDocument::fromJson(read(tx.root() + "/" + result.id + "/manifest.json")).object(); int configRecords = 0;
        for (const auto &file : manifest.value("files").toArray()) configRecords += file.toObject().value("scope").toString() == "shortcuts";
        QCOMPARE(configRecords, 1); QVERIFY(tx.restore(result.id).ok); QCOMPARE(read(config), original);
        QVERIFY(!QFileInfo::exists(Transactions::destination(a.game, "icon"))); QVERIFY(!QFileInfo::exists(Transactions::destination(b.game, "icon")));
    }
    void changedShortcutBlocksRestoreWithoutPartialWrites() {
        QTemporaryDir dir; fixture(dir.path()); const auto config = dir.path() + "/userdata/42/config/shortcuts.vdf"; write(config, shortcuts({0x80000042U}));
        const auto source = dir.path() + "/source.png"; write(source, png(Qt::red)); const auto item = iconChange(dir.path(), source, true, 0x80000042U);
        Transactions tx(dir.path() + "/backups"); const auto result = tx.apply({item}); QVERIFY2(result.ok, qPrintable(result.error));
        const auto icon = read(Transactions::destination(item.game, "icon")); auto changed = read(config); changed.replace("synthetic-option", "changed-by-steam"); write(config, changed);
        QVERIFY(!tx.restore(result.id).ok); QCOMPARE(read(config), changed); QCOMPARE(read(Transactions::destination(item.game, "icon")), icon);
    }
    void malformedShortcutCancelsWholeBatch() {
        QTemporaryDir dir; fixture(dir.path()); write(dir.path() + "/userdata/42/config/shortcuts.vdf", shortcuts({0x80000042U}).chopped(1));
        const auto source = dir.path() + "/source.png"; write(source, png(Qt::red)); Transactions tx(dir.path() + "/backups");
        QVERIFY(!tx.apply({change(dir.path(), source), iconChange(dir.path(), source, true, 0x80000042U)}).ok);
        QVERIFY(!QFileInfo::exists(dir.path() + "/userdata/42/config/grid/100p.png"));
    }
    void icoUsesLargestResolutionAndIsNotAnimation() {
        QTemporaryDir dir; fixture(dir.path()); write(dir.path() + "/appcache/appinfo.vdf", metadata(QString(40, 'a')));
        const auto source = dir.path() + "/multi.ico"; write(source, multiResolutionIco()); bool animated = true; QString error;
        QVERIFY2(Transactions::validImage(source, &animated, &error), qPrintable(error)); QVERIFY(!animated); QVERIFY(Transactions::validIcon(source, &error));
        QImageReader reader(source); QVERIFY(selectLargestIcon(reader)); QCOMPARE(reader.size(), QSize(256, 256));
        Transactions tx(dir.path() + "/backups"); const auto item = iconChange(dir.path(), source); const auto result = tx.apply({item}); QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(QImage(Transactions::destination(item.game, "icon")).pixelColor(128, 128), QColor(Qt::blue)); QVERIFY(tx.restore(result.id).ok);
    }
    void versionOneBackupsStillRestore() {
        QTemporaryDir dir; fixture(dir.path()); const auto source = dir.path() + "/source.png"; write(source, png(Qt::red)); Transactions tx(dir.path() + "/backups"); const auto result = tx.apply({change(dir.path(), source)}); QVERIFY(result.ok);
        const auto path = tx.root() + "/" + result.id + "/manifest.json"; auto manifest = QJsonDocument::fromJson(read(path)).object(); manifest["version"] = 1;
        QJsonArray files; for (const auto &value : manifest.value("files").toArray()) { auto file = value.toObject(); file.remove("scope"); files.append(file); } manifest["files"] = files; write(path, QJsonDocument(manifest).toJson());
        QVERIFY(tx.restore(result.id).ok);
    }
    void externalIconScannerDoesNotModifyShortcut() {
        QTemporaryDir dir; fixture(dir.path()); const auto path = dir.path() + "/external.png"; write(path, png(Qt::blue));
        const auto data = ValveKv::shortcutIcons(shortcuts({0x80000042U}), {{0x80000042U, path}});
        const auto config = dir.path() + "/userdata/42/config/shortcuts.vdf"; write(config, data);
        const auto scan = SteamScanner::scan(dir.path()); QCOMPARE(scan.games.size(), 2); bool found = false;
        for (const auto &game : scan.games) if (game.external) { found = true; QCOMPARE(SteamScanner::existingArt(game, "icon"), path); }
        QVERIFY(found); QCOMPARE(read(config), data);
    }
    void iconDestinationRejectsUnexpectedNames() {
        QTemporaryDir dir; fixture(dir.path());
        QJsonObject record{{"scope", "library-icon"}, {"root", dir.path()}, {"appid", 100}, {"hash", QString(40, 'a')}, {"name", "../../outside.jpg"}};
        QVERIFY(Transactions::recordDestination(record).isEmpty());
        record["name"] = QString(40, 'a') + ".jpg"; record["hash"] = "../../outside"; QVERIFY(Transactions::recordDestination(record).isEmpty());
        record["scope"] = "shortcuts"; record["name"] = "shortcuts.vdf"; record["user"] = "../outside"; QVERIFY(Transactions::recordDestination(record).isEmpty());
    }
    void animatedIconFlagRejectedBeforeWriting() {
        QTemporaryDir dir; fixture(dir.path()); write(dir.path() + "/appcache/appinfo.vdf", metadata(QString(40, 'a')));
        const auto source = dir.path() + "/source.png"; write(source, png(Qt::red)); auto item = iconChange(dir.path(), source); item.animated = true;
        Transactions tx(dir.path() + "/backups"); QVERIFY(!tx.apply({item}).ok); QVERIFY(!QFileInfo::exists(Transactions::destination(item.game, "icon")));
    }
    void indexedSearchNaturalOrderAndFilters() {
        Game a; a.appId = 10; a.name = "Juego 10 — ÁRBOL"; a.installed = true; a.lastPlayed = 9999;
        Game b = a; b.appId = 2; b.name = "Juego 2 — Árbol"; b.installed = false; b.lastPlayed = 0;
        LibraryIndex index; index.setGames({a, b}); LibraryFilter filter; filter.text = "arbol";
        auto result = index.query(filter); QCOMPARE(result.size(), 2); QCOMPARE(result.first().appId, 2U);
        filter.status = 1; QCOMPARE(index.query(filter).size(), 1); filter.status = 0; filter.collectionOnly = true; QCOMPARE(index.query(filter).size(), 0);
        filter.collection.insert(a.key()); QCOMPARE(index.query(filter).first().appId, 10U);
    }
    void diagnosticsUseOnlyAllowlistedTechnicalFields() {
        const auto bytes = diagnosticReport(50000, 1048576, 2, 1, true, true);
        const auto report = QJsonDocument::fromJson(bytes).object();
        QStringList expected = {"format", "application", "version", "qt", "system", "architecture", "local_entries", "cache_bytes", "pending_changes", "active_downloads", "ffmpeg_available", "demo", "privacy"};
        expected.sort(); QCOMPARE(report.keys(), expected); QCOMPARE(report.value("local_entries").toInt(), 50000);
        QVERIFY(!bytes.contains("/home/")); QVERIFY(!bytes.contains("https://")); QVERIFY(!bytes.contains("Bearer"));
    }
    void cacheClearPreservesMarkerAndUnmanagedFiles() {
        QTemporaryDir dir; CacheStore cache(dir.path()); write(dir.path() + "/.steam-art-studio-data", "Steam Art Studio data v1\n"); write(dir.path() + "/notes.txt", "user work"); write(dir.path() + "/backups/original.bin", "backup");
        cache.store(QUrl("https://cdn2.steamgriddb.com/grid/synthetic.png"), "data", "image/png"); cache.clearUnpinned();
        QCOMPARE(read(dir.path() + "/.steam-art-studio-data"), QByteArray("Steam Art Studio data v1\n")); QCOMPARE(read(dir.path() + "/notes.txt"), QByteArray("user work")); QCOMPARE(read(dir.path() + "/backups/original.bin"), QByteArray("backup"));
    }
    void pinnedUrlCannotChangePreparedOriginal() {
        QTemporaryDir dir; CacheStore cache(dir.path()); const QUrl url("https://cdn2.steamgriddb.com/grid/synthetic.png"); const auto path = cache.store(url, "first", "image/png"); cache.pin(path); cache.store(url, "changed", "image/png"); QCOMPARE(read(path), QByteArray("first")); cache.unpin(path); cache.store(url, "changed", "image/png"); QCOMPARE(read(path), QByteArray("changed"));
    }
    void dataAndCacheMustBeDisjoint() {
        QTemporaryDir dir; QVERIFY(!DraftStore::separateDirectories(dir.path(), dir.path())); QVERIFY(!DraftStore::separateDirectories(dir.path(), dir.path() + "/cache")); QVERIFY(DraftStore::separateDirectories(dir.path() + "/data", dir.path() + "/cache"));
        QDir().mkpath(dir.path() + "/data"); QVERIFY(QFile::link(dir.path() + "/data", dir.path() + "/alias")); QVERIFY(!DraftStore::separateDirectories(dir.path() + "/data", dir.path() + "/alias/nested"));
    }
    void draftRecoversAndPreservesCorruptFile() {
        QTemporaryDir dir; const auto data = dir.path() + "/data"; QDir().mkpath(data); CacheStore cache(dir.path() + "/cache"); const auto source = dir.path() + "/source.png"; write(source, png(Qt::red));
        const auto file = cache.importFile(source); auto a = change(dir.path() + "/steam", file), b = a; b.game.appId = 200;
        DraftStore store(data, cache.root()); QVERIFY(store.save({a})); QVERIFY(store.save({a, b})); write(data + "/draft.json", "{broken");
        const auto loaded = store.load(); QCOMPARE(loaded.changes.size(), 1); QVERIFY(!loaded.warning.isEmpty()); QCOMPARE(QDir(data).entryList({"draft.corrupt-*.json"}, QDir::Files).size(), 1); QVERIFY(store.save(loaded.changes));
    }
    void draftRejectsDuplicatesTraversalAndTamperedOriginal() {
        QTemporaryDir dir; QDir().mkpath(dir.path() + "/data"); CacheStore cache(dir.path() + "/cache"); const auto source = dir.path() + "/source.png"; write(source, png(Qt::red)); const auto file = cache.importFile(source); auto item = change(dir.path() + "/steam", file);
        DraftStore store(dir.path() + "/data", cache.root()); QVERIFY(store.save({item, item})); auto loaded = store.load(); QCOMPARE(loaded.changes.size(), 1); QCOMPARE(loaded.rejected, 1);
        write(file, png(Qt::blue)); loaded = store.load(); QCOMPARE(loaded.changes.size(), 0); QCOMPARE(loaded.rejected, 2);
    }
    void draftBackupFailureDoesNotOverwriteSavedQueue() {
        QTemporaryDir dir; QDir().mkpath(dir.path() + "/data"); CacheStore cache(dir.path() + "/cache"); DraftStore store(dir.path() + "/data", cache.root()); QVERIFY(store.save({})); const auto before = read(dir.path() + "/data/draft.json"); QDir().mkpath(dir.path() + "/data/draft.previous.json"); QString error; QVERIFY(!store.save({}, &error)); QVERIFY(!error.isEmpty()); QCOMPARE(read(dir.path() + "/data/draft.json"), before);
    }
    void previousDraftOriginalsSurviveCacheCleanupWhenPinned() {
        QTemporaryDir dir; QDir().mkpath(dir.path() + "/data"); CacheStore cache(dir.path() + "/cache");
        write(dir.path() + "/image.png", png(Qt::red)); const auto file = cache.importFile(dir.path() + "/image.png");
        DraftStore store(dir.path() + "/data", cache.root()); QVERIFY(store.save({change(dir.path() + "/steam", file)})); QVERIFY(store.save({}));
        QCOMPARE(store.previousOriginals(), QStringList{file}); for (const auto &path : store.previousOriginals()) cache.pin(path);
        cache.clearUnpinned(); QVERIFY(QFileInfo::exists(file)); write(dir.path() + "/data/draft.json", "broken"); QCOMPARE(store.load().changes.size(), 1);
    }
    void failedSecondWriteRollsBackWithoutRewritingUntouchedFile() {
        QTemporaryDir dir; fixture(dir.path() + "/steam"); const QString grid = dir.path() + "/steam/userdata/42/config/grid/"; const auto blue = png(Qt::blue), green = png(Qt::green); write(grid + "100p.png", blue); write(grid + "200p.png", green); write(dir.path() + "/new.png", png(Qt::red));
        auto a = change(dir.path() + "/steam", dir.path() + "/new.png"), b = a; b.game.appId = 200;
        TransactionIo io; io.write = [](const QString &path, const QByteArray &bytes, QString *error) { if (path.endsWith("/grid/200p.png")) { if (error) *error = "Permiso denegado simulado"; return false; } return CacheStore::atomicWrite(path, bytes, error); };
        Transactions tx(dir.path() + "/backups", io); const auto result = tx.apply({a, b}); QVERIFY(!result.ok); QCOMPARE(read(grid + "100p.png"), blue); QCOMPARE(read(grid + "200p.png"), green); QCOMPARE(tx.history().first().toObject().value("status").toString(), "restored");
    }
    void failedBackupAndFinalJournalKeepOriginalsRecoverable() {
        QTemporaryDir dir; fixture(dir.path() + "/steam"); const auto source = dir.path() + "/new.png", target = dir.path() + "/steam/userdata/42/config/grid/100p.png"; write(source, png(Qt::red)); const auto original = png(Qt::blue); write(target, original);
        TransactionIo noSpace; noSpace.write = [](const QString &path, const QByteArray &bytes, QString *error) { if (path.endsWith(".bin")) { if (error) *error = "Disco lleno simulado"; return false; } return CacheStore::atomicWrite(path, bytes, error); };
        Transactions failing(dir.path() + "/backups", noSpace); QVERIFY(!failing.apply({change(dir.path() + "/steam", source)}).ok); QCOMPARE(read(target), original);
        TransactionIo journal; journal.write = [](const QString &path, const QByteArray &bytes, QString *error) { if (path.endsWith("manifest.json") && QJsonDocument::fromJson(bytes).object().value("status").toString() == "committed") { if (error) *error = "Fallo de registro simulado"; return false; } return CacheStore::atomicWrite(path, bytes, error); };
        Transactions partial(dir.path() + "/backups", journal); const auto applied = partial.apply({change(dir.path() + "/steam", source)}); QVERIFY(!applied.ok);
        Transactions recovered(dir.path() + "/backups"); QVERIFY(recovered.restore(applied.id).ok); QCOMPARE(read(target), original);
    }
    void abruptProcessExitDuringApplyAndRestore() {
        QTemporaryDir dir("/tmp/opencode/studio-fault-XXXXXX"); QVERIFY(dir.isValid()); fixture(dir.path() + "/steam"); write(dir.path() + "/.synthetic-fixture", "synthetic-only\n"); write(dir.path() + "/new.png", png(Qt::red));
        const QString grid = dir.path() + "/steam/userdata/42/config/grid/"; const auto blue = png(Qt::blue), green = png(Qt::green); write(grid + "100p.png", blue); write(grid + "200p.png", green);
        const QString worker = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../../build-crash-tests/bin/studio-crash-worker"); QVERIFY2(QFileInfo(worker).isExecutable(), "Compila el worker con scripts/test.sh");
        QProcess child; child.start(worker, {dir.path(), "apply"}); QVERIFY(child.waitForFinished(10000)); QCOMPARE(child.exitCode(), 86);
        Transactions tx(dir.path() + "/backups"); const auto history = tx.history(); QCOMPARE(history.size(), 1); const auto id = history.first().toObject().value("id").toString(); QCOMPARE(history.first().toObject().value("status").toString(), "applying"); QVERIFY(tx.restore(id).ok); QCOMPARE(read(grid + "100p.png"), blue); QCOMPARE(read(grid + "200p.png"), green);
        auto a = change(dir.path() + "/steam", dir.path() + "/new.png"), b = a; b.game.appId = 200; const auto applied = tx.apply({a, b}); QVERIFY(applied.ok);
        child.start(worker, {dir.path(), "restore", applied.id}); QVERIFY(child.waitForFinished(10000)); QCOMPARE(child.exitCode(), 86); QVERIFY(tx.restore(applied.id).ok); QCOMPARE(read(grid + "100p.png"), blue); QCOMPARE(read(grid + "200p.png"), green);
    }
};
QTEST_GUILESS_MAIN(StudioTests)
#include "test_studio.moc"
