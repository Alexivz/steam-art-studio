#include "network.h"
#include "transfers.h"
#include "fakenetwork.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
using namespace Studio;
namespace {
QUrl image(int i = 1) { return QUrl("https://cdn2.steamgriddb.com/grid/synthetic-" + QString::number(i) + ".png"); }
QUrl query() { return QUrl("https://www.steamgriddb.com/api/v2/grids/game/100"); }
NetworkPolicy policy() { NetworkPolicy limits{1000, 1000, 2000, 2}; limits.apiResponseMs = limits.apiTransferMs = 1000; limits.apiDeadlineMs = 2000; return limits; }
QByteArray read(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }
FakeResponse api(int status, const QByteArray &body) { FakeResponse r; r.status = status; r.body = body; r.mime = "application/json"; return r; }
}
class NetworkTests : public QObject {
    Q_OBJECT
private slots:
    void cachedDeliveryCanBeCancelled() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); const auto path = cache.store(image(), "old", "image/png"); FakeNetwork transport; Network network(&cache, nullptr, &transport, policy());
        QSignalSpy ready(&network, &Network::ready); QSignalSpy failed(&network, &Network::failed);
        const auto token = network.fetch(image()); network.cancel(token); QTest::qWait(10); QCOMPARE(ready.size(), 0); QCOMPARE(failed.size(), 0); QCOMPARE(transport.requests.size(), 0);
        cache.clearUnpinned(); QVERIFY(!QFileInfo::exists(path));
    }
    void deduplicatedSubscribersCancelIndependently() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse r; r.delay = 10; transport.responses.enqueue(r); Network network(&cache, nullptr, &transport, policy()); QSignalSpy ready(&network, &Network::ready);
        const auto a = network.fetch(image()), b = network.fetch(image()); network.cancel(a); QTRY_COMPARE(ready.size(), 1); QCOMPARE(ready.first().at(0).toULongLong(), b); QCOMPARE(transport.requests.size(), 1);
    }
    void concurrencyAndCancelAll() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse stalled; stalled.stall = true; for (int i = 0; i < 10; ++i) transport.responses.enqueue(stalled);
        Network network(&cache, nullptr, &transport, policy()); network.setApiKey(QString(32, 'x')); // Synthetic, in memory only; never a real key.
        for (int i = 0; i < 6; ++i) network.fetch(image(i));
        for (int i = 0; i < 4; ++i) network.fetch(QUrl("https://www.steamgriddb.com/api/v2/games/steam/" + QString::number(i + 1)), "api");
        QCOMPARE(transport.peak, 4); QCOMPARE(transport.peakImages, 2); const int sent = transport.requests.size(); network.cancelAll(); QCOMPARE(transport.active, 0); QTest::qWait(10); QCOMPARE(transport.requests.size(), sent);
    }
    void retryIsBoundedAndCancellationCannotResurrectJob() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse bad; bad.status = 500; transport.responses.enqueue(bad); transport.responses.enqueue(FakeResponse{});
        auto limits = policy(); limits.retryMs = 200; Network network(&cache, nullptr, &transport, limits); QSignalSpy ready(&network, &Network::ready);
        const auto old = network.fetch(image()); QTRY_COMPARE(transport.active, 0); network.cancel(old); const auto next = network.fetch(image()); QTRY_COMPARE(ready.size(), 1); QTest::qWait(220);
        QCOMPARE(ready.first().at(0).toULongLong(), next); QCOMPARE(transport.requests.size(), 2);
    }
    void serverErrorRetriesOnce() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse bad; bad.status = 500; transport.responses.enqueue(bad); transport.responses.enqueue(bad); Network network(&cache, nullptr, &transport, policy()); QSignalSpy failed(&network, &Network::failed);
        network.fetch(image()); QTRY_COMPARE(failed.size(), 1); QCOMPARE(transport.requests.size(), 2);
    }
    void rateLimitHonorsRetryAfter() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse limited; limited.status = 429; limited.retryAfter = "7200"; transport.responses.enqueue(limited); Network network(&cache, nullptr, &transport, policy()); QSignalSpy failed(&network, &Network::failed);
        network.fetch(image()); QTRY_COMPARE(failed.size(), 1); network.fetch(image(2)); QTRY_COMPARE(failed.size(), 2); QCOMPARE(transport.requests.size(), 1); QCOMPARE(failed.last().at(2).toInt(), 429);
        const auto seconds = QRegularExpression("(\\d+) s").match(failed.last().at(1).toString()); QVERIFY(seconds.hasMatch()); QVERIFY(seconds.captured(1).toLongLong() > 3600);
    }
    void queuedRequestsAlsoHonorRetryAfter() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse limited; limited.status = 429; limited.retryAfter = "60"; transport.responses.enqueue(limited);
        FakeResponse slow; slow.delay = 100; transport.responses.enqueue(slow);
        Network network(&cache, nullptr, &transport, policy()); QSignalSpy failed(&network, &Network::failed); QSignalSpy ready(&network, &Network::ready);
        network.fetch(image(1)); network.fetch(image(2)); network.fetch(image(3));
        QTRY_COMPARE(failed.size(), 2); QTRY_COMPARE(ready.size(), 1); QCOMPARE(transport.requests.size(), 2);
    }
    void deferredAbortCannotCaptureNewSubscriberOrRedirect() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse stalled; stalled.stall = true; stalled.abortDelay = 20; stalled.status = 302; stalled.redirect = image(2);
        transport.responses.enqueue(stalled); transport.responses.enqueue(FakeResponse{});
        Network network(&cache, nullptr, &transport, policy()); QSignalSpy ready(&network, &Network::ready); QSignalSpy failed(&network, &Network::failed);
        const auto old = network.fetch(image()); network.cancel(old); const auto next = network.fetch(image());
        QTRY_COMPARE(ready.size(), 1); QTest::qWait(30); QCOMPARE(ready.first().at(0).toULongLong(), next);
        QCOMPARE(failed.size(), 0); QCOMPARE(transport.requests.size(), 2); QCOMPARE(transport.active, 0);
    }
    void rejectedAuthenticationNeverFallsBack() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; transport.responses.enqueue(api(401, "{\"success\":false}")); Network network(&cache, nullptr, &transport, policy()); network.setApiKey(QString(32, 'x'));
        ArtService service(&network); QSignalSpy error(&service, &ArtService::error); Game game; game.appId = 100; game.name = "Synthetic"; service.search(game, "capsule", true, false, false, 0);
        QTRY_COMPARE(error.size(), 1); QCOMPARE(transport.requests.size(), 1);
    }
    void onlyMissingGameFallsBackToTitle() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        transport.responses.enqueue(api(404, "{\"success\":false}")); transport.responses.enqueue(api(200, "{\"success\":true,\"data\":[{\"id\":77,\"name\":\"Synthetic\"}]}")); transport.responses.enqueue(api(200, "{\"success\":true,\"data\":[],\"total\":0}"));
        Network network(&cache, nullptr, &transport, policy()); network.setApiKey(QString(32, 'x')); ArtService service(&network); QSignalSpy results(&service, &ArtService::results); Game game; game.appId = 100; game.name = "Synthetic"; service.search(game, "icon", true, false, false, 0);
        QTRY_COMPARE(results.size(), 1); QCOMPARE(transport.requests.size(), 3); QCOMPARE(transport.requests.last().url().path(), "/api/v2/icons/game/77");
    }
    void unsafeRedirectNeverReceivesRequestOrKey() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse redirect; redirect.status = 302; redirect.redirect = QUrl("https://outside.invalid/api/v2/steal"); transport.responses.enqueue(redirect);
        Network network(&cache, nullptr, &transport, policy()); network.setApiKey(QString(32, 'x')); QSignalSpy failed(&network, &Network::failed); network.fetch(QUrl("https://www.steamgriddb.com/api/v2/games/steam/100"), "api");
        QTRY_COMPARE(failed.size(), 1); QCOMPARE(transport.requests.size(), 1);
    }
    void imagesNeverReceiveAuthorization() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; transport.responses.enqueue(FakeResponse{}); Network network(&cache, nullptr, &transport, policy()); network.setApiKey(QString(32, 'x')); QSignalSpy ready(&network, &Network::ready);
        network.fetch(image()); QTRY_COMPARE(ready.size(), 1); QVERIFY(!transport.requests.first().hasRawHeader("Authorization"));
        for (const auto &file : QDir(dir.path() + "/metadata").entryList({"*.json"}, QDir::Files)) QVERIFY(!read(dir.path() + "/metadata/" + file).contains("xxxxxxxx"));
    }
    void oversizedThumbnailIsRejected() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse huge; huge.body = QByteArray(8 * 1024 * 1024 + 1, 'x'); huge.chunk = 1024 * 1024; transport.responses.enqueue(huge); Network network(&cache, nullptr, &transport, policy()); QSignalSpy failed(&network, &Network::failed);
        network.fetch(image()); QTRY_COMPARE(failed.size(), 1); QCOMPARE(transport.requests.size(), 1); QVERIFY(!cache.lookup(image(), 60).valid());
    }
    void timeoutIsActionable() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse stalled; stalled.stall = true; transport.responses.enqueue(stalled); auto limits = policy(); limits.deadlineMs = 20; Network network(&cache, nullptr, &transport, limits); QSignalSpy failed(&network, &Network::failed);
        network.fetch(image()); QTRY_COMPARE(failed.size(), 1); QVERIFY(failed.first().at(1).toString().contains("tiempo de espera"));
    }
    void originalDefaultsAllowLargerDownloadsWithoutWeakeningOtherLimits() {
        const NetworkPolicy limits;
        QCOMPARE(limits.deadlineMs, 30000); QCOMPARE(limits.transferMs, 15000);
        QCOMPARE(limits.originalConnectionMs, 15000); QCOMPARE(limits.originalTransferMs, 30000); QCOMPARE(limits.originalDeadlineMs, 180000);
        QCOMPARE(limits.apiResponseMs, 30000); QCOMPARE(limits.apiTransferMs, 30000); QCOMPARE(limits.apiDeadlineMs, 90000);
    }
    void apiWaitsForResponseOnNewAndReusedTls_data() {
        QTest::addColumn<bool>("encrypted"); QTest::newRow("new-tls") << true; QTest::newRow("reused-tls-no-encrypted-signal") << false;
    }
    void apiWaitsForResponseOnNewAndReusedTls() {
        QFETCH(bool, encrypted);
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        auto slow = api(200, "{\"success\":true,\"data\":[]}"); slow.delay = 150; slow.encrypted = encrypted; transport.responses.enqueue(slow);
        auto limits = policy(); limits.connectionMs = 20; limits.apiResponseMs = limits.apiTransferMs = 600;
        Network network(&cache, nullptr, &transport, limits); network.setApiKey(QString(32, 'x'));
        QSignalSpy ready(&network, &Network::ready); QSignalSpy failed(&network, &Network::failed);
        network.fetch(query(), "api"); QTest::qWait(60); QCOMPARE(ready.size(), 0); QCOMPARE(failed.size(), 0);
        QTRY_COMPARE(ready.size(), 1); QCOMPARE(failed.size(), 0); QCOMPARE(transport.requests.size(), 1);
    }
    void apiTimeoutRetriesOnceThenManualRetryCanSucceed() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse stalled; stalled.status = 0; stalled.stall = true; stalled.metadata = false; stalled.encrypted = true;
        transport.responses.enqueue(stalled); transport.responses.enqueue(stalled); transport.responses.enqueue(api(200, "{\"success\":true,\"data\":[]}"));
        auto limits = policy(); limits.apiResponseMs = 60;
        Network network(&cache, nullptr, &transport, limits); network.setApiKey(QString(32, 'x'));
        ArtService service(&network); Game game; game.appId = 100; service.setMapping(game.key(), 100);
        QSignalSpy results(&service, &ArtService::results); QSignalSpy errors(&service, &ArtService::error);
        service.search(game, "capsule", true, false, false, 0); QTRY_COMPARE(errors.size(), 1);
        QCOMPARE(results.size(), 0); QCOMPARE(transport.requests.size(), 2);
        QVERIFY(errors.first().first().toString().contains("Reintentar búsqueda")); QVERIFY(!errors.first().first().toString().contains("Descargas"));
        service.search(game, "capsule", true, false, false, 0); QTRY_COMPARE(results.size(), 1); QCOMPARE(transport.requests.size(), 3);
    }
    void apiIdleTimeoutDiscardsPartialResponseBeforeRetry() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        const auto body = QByteArray("{\"success\":true,\"data\":[]}");
        auto partial = api(200, body); partial.chunk = 5; partial.stallAfterBytes = 5;
        transport.responses.enqueue(partial); transport.responses.enqueue(api(200, body));
        auto limits = policy(); limits.apiTransferMs = 60;
        Network network(&cache, nullptr, &transport, limits); network.setApiKey(QString(32, 'x'));
        QSignalSpy ready(&network, &Network::ready); QSignalSpy failed(&network, &Network::failed);
        network.fetch(query(), "api"); QTRY_COMPARE(ready.size(), 1); QCOMPARE(failed.size(), 0);
        QCOMPARE(read(ready.first().at(1).toString()), body); QCOMPARE(transport.requests.size(), 2);
    }
    void apiTotalDeadlineIncludesRetryAndCannotLoop() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse stalled; stalled.status = 0; stalled.stall = true; stalled.metadata = false;
        transport.responses.enqueue(stalled); transport.responses.enqueue(stalled);
        auto limits = policy(); limits.apiResponseMs = 160; limits.apiDeadlineMs = 260; limits.retryMs = 10;
        Network network(&cache, nullptr, &transport, limits); network.setApiKey(QString(32, 'x')); QSignalSpy failed(&network, &Network::failed);
        QElapsedTimer elapsed; elapsed.start(); network.fetch(query(), "api"); QTRY_COMPARE(failed.size(), 1);
        QVERIFY(failed.first().at(1).toString().contains("tiempo de espera total de la consulta")); QVERIFY(elapsed.elapsed() < 800);
        QTest::qWait(100); QCOMPARE(transport.requests.size(), 2); QVERIFY(!cache.lookup(query(), 60).valid());
    }
    void apiProgressDoesNotExtendTotalDeadline() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        auto slow = api(200, QByteArray(10000, 'x')); slow.chunk = 1; slow.delay = 5; transport.responses.enqueue(slow);
        auto limits = policy(); limits.apiDeadlineMs = 100;
        Network network(&cache, nullptr, &transport, limits); network.setApiKey(QString(32, 'x')); QSignalSpy failed(&network, &Network::failed);
        network.fetch(query(), "api"); QTRY_COMPARE(failed.size(), 1); QCOMPARE(transport.requests.size(), 1); QVERIFY(!cache.lookup(query(), 60).valid());
    }
    void apiRedirectDoesNotResetTotalDeadline() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        auto redirect = api(302, ""); redirect.delay = 160; redirect.redirect = QUrl("https://www.steamgriddb.com/api/v2/grids/game/101");
        transport.responses.enqueue(redirect); FakeResponse stalled; stalled.status = 0; stalled.metadata = false; stalled.stall = true; transport.responses.enqueue(stalled);
        auto limits = policy(); limits.apiDeadlineMs = 260;
        Network network(&cache, nullptr, &transport, limits); network.setApiKey(QString(32, 'x')); QSignalSpy failed(&network, &Network::failed);
        network.fetch(query(), "api"); QTRY_COMPARE(failed.size(), 1); QCOMPARE(transport.requests.size(), 2);
        QVERIFY(failed.first().at(1).toString().contains("tiempo de espera total de la consulta")); QVERIFY(!cache.lookup(query(), 60).valid());
    }
    void oversizedApiResponseIsNeverCachedOrRetried() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        auto huge = api(200, QByteArray(2 * 1024 * 1024 + 1, 'x')); transport.responses.enqueue(huge);
        Network network(&cache, nullptr, &transport, policy()); network.setApiKey(QString(32, 'x')); QSignalSpy failed(&network, &Network::failed);
        network.fetch(query(), "api"); QTRY_COMPARE(failed.size(), 1); QCOMPARE(transport.requests.size(), 1); QVERIFY(!cache.lookup(query(), 60).valid());
    }
    void changingSearchCancelsUnansweredApiWithoutLateError() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse stalled; stalled.status = 0; stalled.stall = true; stalled.metadata = false; stalled.abortDelay = 30;
        transport.responses.enqueue(stalled); transport.responses.enqueue(api(200, "{\"success\":true,\"data\":[]}"));
        auto limits = policy(); limits.apiResponseMs = 60;
        Network network(&cache, nullptr, &transport, limits); network.setApiKey(QString(32, 'x')); ArtService service(&network);
        QSignalSpy results(&service, &ArtService::results); QSignalSpy errors(&service, &ArtService::error);
        Game old; old.appId = 100; Game next; next.appId = 101; service.setMapping(old.key(), 100); service.setMapping(next.key(), 101);
        service.search(old, "capsule", true, false, false, 0); service.search(next, "capsule", true, false, false, 0);
        QTRY_COMPARE(results.size(), 1); QTest::qWait(100); QCOMPARE(errors.size(), 0); QCOMPARE(transport.requests.size(), 2); QCOMPARE(transport.active, 0);
    }
    void apiCancellationDuringTimeoutBackoffCannotResurrectQuery() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse stalled; stalled.status = 0; stalled.stall = true; stalled.metadata = false;
        transport.responses.enqueue(stalled); transport.responses.enqueue(api(200, "{\"success\":true,\"data\":[]}"));
        auto limits = policy(); limits.apiResponseMs = 60; limits.retryMs = 250;
        Network network(&cache, nullptr, &transport, limits); network.setApiKey(QString(32, 'x'));
        QSignalSpy ready(&network, &Network::ready); QSignalSpy failed(&network, &Network::failed); QSignalSpy cancelled(&network, &Network::cancelled);
        const auto old = network.fetch(query(), "api"); QTRY_COMPARE(transport.active, 0); network.cancel(old);
        const auto next = network.fetch(query(), "api"); QTRY_COMPARE(ready.size(), 1); QTest::qWait(300);
        QCOMPARE(ready.first().first().toULongLong(), next); QCOMPARE(cancelled.size(), 1); QCOMPARE(failed.size(), 0); QCOMPARE(transport.requests.size(), 2);
    }
    void apiTlsAuthAndRateLimitNeverRetry_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<int>("error");
        QTest::newRow("tls") << 0 << int(QNetworkReply::SslHandshakeFailedError);
        QTest::newRow("unauthorized") << 401 << int(QNetworkReply::AuthenticationRequiredError);
        QTest::newRow("forbidden") << 403 << int(QNetworkReply::ContentAccessDenied);
        QTest::newRow("rate-limit") << 429 << int(QNetworkReply::UnknownContentError);
    }
    void apiTlsAuthAndRateLimitNeverRetry() {
        QFETCH(int, status); QFETCH(int, error);
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        auto bad = api(status, "{\"success\":false}"); bad.error = QNetworkReply::NetworkError(error); bad.retryAfter = "60"; transport.responses.enqueue(bad);
        Network network(&cache, nullptr, &transport, policy()); network.setApiKey(QString(32, 'x')); QSignalSpy failed(&network, &Network::failed);
        network.fetch(query(), "api"); QTRY_COMPARE(failed.size(), 1); QCOMPARE(transport.requests.size(), 1); QVERIFY(!cache.lookup(query(), 60).valid());
    }
    void progressingOriginalSurvivesThumbnailDeadline() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse slow; slow.body = QByteArray(8192, 'a'); slow.chunk = 1024; slow.delay = 25;
        transport.responses.enqueue(slow); transport.responses.enqueue(slow);
        auto limits = policy(); limits.deadlineMs = 80; limits.originalConnectionMs = limits.originalTransferMs = 1000; limits.originalDeadlineMs = 1500;
        Network network(&cache, nullptr, &transport, limits); QSignalSpy failed(&network, &Network::failed); QSignalSpy ready(&network, &Network::ready);
        const auto thumbnail = network.fetch(image(1)), original = network.fetch(image(2), "original");
        QTRY_COMPARE(failed.size(), 1); QCOMPARE(failed.first().first().toULongLong(), thumbnail); QVERIFY(!cache.lookup(image(1), 60).valid());
        QTRY_COMPARE(ready.size(), 1); QCOMPARE(ready.first().first().toULongLong(), original); QCOMPARE(read(ready.first().at(1).toString()), slow.body);
        QCOMPARE(transport.requests.size(), 2); QVERIFY(!transport.requests.last().hasRawHeader("Authorization"));
    }
    void originalDeadlineRemainsFiniteWhileBytesArrive() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse slow; slow.body = QByteArray(65536, 'a'); slow.chunk = 64; slow.delay = 5; transport.responses.enqueue(slow);
        auto limits = policy(); limits.originalConnectionMs = limits.originalTransferMs = 1000; limits.originalDeadlineMs = 80;
        Network network(&cache, nullptr, &transport, limits); QSignalSpy failed(&network, &Network::failed); QSignalSpy ready(&network, &Network::ready); QSignalSpy progress(&network, &Network::progress);
        network.fetch(image(), "original"); QTRY_COMPARE(failed.size(), 1); QVERIFY(!progress.isEmpty()); QCOMPARE(ready.size(), 0);
        QVERIFY(failed.first().at(1).toString().contains("tiempo de espera total")); QVERIFY(!cache.lookup(image(), 60).valid()); QCOMPARE(transport.requests.size(), 1);
    }
    void stalledOriginalPreservesProgressAndCanRetryWithoutPartialCache() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse stalled; stalled.body = QByteArray(8192, 'a'); stalled.chunk = 1024; stalled.stallAfterBytes = 1024; transport.responses.enqueue(stalled);
        FakeResponse complete; complete.body = "complete-original"; transport.responses.enqueue(complete);
        auto limits = policy(); limits.originalConnectionMs = 1000; limits.originalTransferMs = 40; limits.originalDeadlineMs = 1500;
        Network network(&cache, nullptr, &transport, limits); TransferModel tasks(&network); QSignalSpy completed(&tasks, &TransferModel::completed);
        QVERIFY(tasks.enqueue(image(), "Synthetic animation", {}));
        QTRY_COMPARE(tasks.index(0).data(TransferModel::StateRole).toInt(), int(TransferModel::Failed));
        QCOMPARE(tasks.index(0).data(TransferModel::ReceivedRole).toLongLong(), 1024);
        QVERIFY(tasks.index(0).data().toString().contains("KiB")); QVERIFY(tasks.index(0).data(TransferModel::ErrorRole).toString().contains("no se recibieron datos"));
        QVERIFY(!cache.lookup(image(), 60).valid()); QCOMPARE(completed.size(), 0);
        tasks.retry(0); QTRY_COMPARE(completed.size(), 1); QCOMPARE(read(cache.lookup(image(), 60).file), complete.body); QCOMPARE(transport.requests.size(), 2);
    }
    void originalConnectionTimeoutIsDistinct() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse stalled; stalled.stall = true; stalled.metadata = false; transport.responses.enqueue(stalled);
        auto limits = policy(); limits.originalConnectionMs = 20; limits.originalTransferMs = 1000; limits.originalDeadlineMs = 1500;
        Network network(&cache, nullptr, &transport, limits); QSignalSpy failed(&network, &Network::failed);
        network.fetch(image(), "original"); QTRY_COMPARE(failed.size(), 1);
        QVERIFY(failed.first().at(1).toString().contains("iniciar la conexión")); QCOMPARE(transport.requests.size(), 1); QVERIFY(!cache.lookup(image(), 60).valid());
    }
    void oversizedOriginalIsStillRejectedWithoutRetryOrPartialCache() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse huge; huge.body = QByteArray(64 * 1024 * 1024 + 1, 'x'); huge.chunk = 1024 * 1024; transport.responses.enqueue(huge);
        Network network(&cache, nullptr, &transport, policy()); QSignalSpy failed(&network, &Network::failed);
        network.fetch(image(), "original"); QTRY_COMPARE(failed.size(), 1); QVERIFY(failed.first().at(1).toString().contains("tamaño máximo"));
        QCOMPARE(transport.requests.size(), 1); QVERIFY(!cache.lookup(image(), 60).valid());
    }
    void qtTimeoutIsReportedAsTimeoutRatherThanTlsFailure() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport;
        FakeResponse timeout; timeout.status = 0; timeout.error = QNetworkReply::TimeoutError; transport.responses.enqueue(timeout);
        Network network(&cache, nullptr, &transport, policy()); QSignalSpy failed(&network, &Network::failed);
        network.fetch(image(), "original"); QTRY_COMPARE(failed.size(), 1);
        QVERIFY(failed.first().at(1).toString().contains("tiempo de espera")); QCOMPARE(transport.requests.size(), 1); QVERIFY(!cache.lookup(image(), 60).valid());
    }
    void transferProgressCancelAndRetry() {
        QTemporaryDir dir("/tmp/opencode/studio-net-XXXXXX"); CacheStore cache(dir.path()); FakeNetwork transport; FakeResponse slow; slow.body = QByteArray(10000, 'x'); slow.chunk = 100; slow.delay = 2; transport.responses.enqueue(slow); transport.responses.enqueue(FakeResponse{});
        Network network(&cache, nullptr, &transport, policy()); TransferModel tasks(&network); QSignalSpy completed(&tasks, &TransferModel::completed);
        QVERIFY(tasks.enqueue(image(), "Synthetic", {})); QTRY_VERIFY(tasks.index(0).data(TransferModel::ReceivedRole).toLongLong() > 0); tasks.cancel(0); QCOMPARE(tasks.activeCount(), 0); QCOMPARE(completed.size(), 0);
        tasks.retry(0); QTRY_COMPARE(completed.size(), 1); QCOMPARE(tasks.index(0).data(TransferModel::StateRole).toInt(), int(TransferModel::Complete)); tasks.clearFinished(); QCOMPARE(tasks.rowCount(), 0);
    }
};
QTEST_GUILESS_MAIN(NetworkTests)
#include "test_network.moc"
