#include "window.h"
#include <QtTest>
#include <QInputDialog>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QBuffer>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTabWidget>
#include <QStackedWidget>
#include <QScrollBar>
#include "fakenetwork.h"
#include "preview.h"

using namespace Studio;
namespace {
QPushButton *action(Window &window, const QString &id) {
    for (auto *button : window.findChildren<QPushButton *>())
        if (button->property("testId").toString() == id) return button;
    return nullptr;
}
void section(Window &window, const QString &name) {
    auto *scroll = window.findChild<QScrollArea *>("workspaceScroll"); auto *target = window.findChild<QWidget *>(name + "Section");
    QVERIFY(scroll); QVERIFY(target); scroll->verticalScrollBar()->setValue(target->mapTo(scroll->widget(), QPoint()).y()); QCoreApplication::processEvents();
}
void format(Window &window, const QString &type) {
    auto *choice = window.findChild<QComboBox *>("artFormat"); QVERIFY(choice); const int row = choice->findData(type); QVERIFY(row >= 0); choice->setCurrentIndex(row);
}
}
class GuiTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QFile version(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../VERSION")); QVERIFY(version.open(QIODevice::ReadOnly)); QCoreApplication::setApplicationVersion(QString::fromLatin1(version.readAll()).trimmed());
    }
    void searchAndSelection() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        Window window(temp.path() + "/data", temp.path() + "/cache", true);
        window.show();
        auto *search = window.findChild<QLineEdit *>("librarySearch");
        auto *library = window.findChild<QListView *>("libraryView");
        auto *gallery = window.findChild<QListView *>("artGallery");
        QVERIFY(search); QVERIFY(library); QVERIFY(gallery);
        QCOMPARE(library->model()->rowCount(), 15);
        search->setText("nEoN");
        QCOMPARE(library->model()->rowCount(), 1);
        QCOMPARE(library->currentIndex().data().toString(), "Neon Horizon");
        QCOMPARE(window.findChild<QLabel *>("gameTitle")->text(), "Neon Horizon");
        QCOMPARE(gallery->model()->rowCount(), 9);
        search->setText("no-such-game");
        QCOMPARE(library->model()->rowCount(), 0);
        QCOMPARE(gallery->model()->rowCount(), 0);
        search->clear();
        QCOMPARE(library->model()->rowCount(), 15);
        QVERIFY(library->currentIndex().isValid());
    }
    void queueReplacementAndPersistence() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        const auto data = temp.path() + "/data", cache = temp.path() + "/cache";
        QString queuedFile;
        {
            Window window(data, cache, true);
            auto *gallery = window.findChild<QListView *>("artGallery");
            auto *queue = window.findChild<QListWidget *>("queueList");
            QVERIFY(action(window, "prepareArtwork"));
            gallery->setCurrentIndex(gallery->model()->index(0, 0));
            action(window, "prepareArtwork")->click();
            QCOMPARE(queue->count(), 1);
            gallery->setCurrentIndex(gallery->model()->index(1, 0));
            action(window, "prepareArtwork")->click();
            QCOMPARE(queue->count(), 1);
            QFile draft(data + "/draft.json"); QVERIFY(draft.open(QIODevice::ReadOnly));
            const auto entries = QJsonDocument::fromJson(draft.readAll()).array();
            QCOMPARE(entries.size(), 1); queuedFile = entries.first().toObject().value("file").toString();
            QVERIFY(QFileInfo::exists(queuedFile));
            // Demo must never permit applying a synthetic target to Steam.
            QVERIFY(!action(window, "applyQueue")->isEnabled());
        }
        {
            Window reopened(data, cache, true);
            auto *queue = reopened.findChild<QListWidget *>("queueList");
            QCOMPARE(queue->count(), 1);
            QVERIFY(queue->item(0)->text().contains("Afterlight"));
            queue->setCurrentRow(0); action(reopened, "removeQueued")->click();
            QCOMPARE(queue->count(), 0);
        }
        Window empty(data, cache, true);
        QCOMPARE(empty.findChild<QListWidget *>("queueList")->count(), 0);
        // No fixture needs a Steam installation or a network key.
        QVERIFY(!QFileInfo::exists(temp.path() + "/steam"));
    }
    void favoritesPersist() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        const auto data = temp.path() + "/data", cache = temp.path() + "/cache";
        {
            Window window(data, cache, true);
            action(window, "toggleFavorite")->click();
            window.findChild<QCheckBox *>("favoritesFilter")->setChecked(true);
            QCOMPARE(window.findChild<QListView *>("libraryView")->model()->rowCount(), 2);
        }
        Window reopened(data, cache, true);
        reopened.findChild<QCheckBox *>("favoritesFilter")->setChecked(true);
        QCOMPARE(reopened.findChild<QListView *>("libraryView")->model()->rowCount(), 2);
    }
    void collectionsPersist() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        const auto data = temp.path() + "/data", cache = temp.path() + "/cache";
        {
            Window window(data, cache, true); window.show();
            window.findChild<QLineEdit *>("newCollectionName")->setText("Mi colección"); action(window, "newCollection")->click();
            action(window, "addToCollection")->click(); QVERIFY(!QApplication::activeModalWidget());
        }
        Window reopened(data, cache, true);
        auto *collections = reopened.findChild<QComboBox *>("collectionFilter");
        QVERIFY(collections); collections->setCurrentText("Mi colección");
        auto *library = reopened.findChild<QListView *>("libraryView");
        QCOMPARE(library->model()->rowCount(), 1);
        QCOMPARE(library->model()->index(0, 0).data().toString(), "Afterlight");
    }
    void sortingAndCardSizePersist() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        const auto data = temp.path() + "/data", cache = temp.path() + "/cache";
        {
            Window window(data, cache, true);
            auto *library = window.findChild<QListView *>("libraryView");
            auto *sort = window.findChild<QComboBox *>("librarySort");
            auto *density = window.findChild<QComboBox *>("cardDensity");
            QCOMPARE(library->model()->index(0, 0).data().toString(), "Afterlight");
            sort->setCurrentIndex(1);
            QCOMPARE(library->model()->index(0, 0).data().toString(), "Wildhaven");
            sort->setCurrentIndex(3);
            bool foundUninstalled = false;
            for (int i = 0; i < library->model()->rowCount(); ++i) {
                const bool installed = library->model()->index(i, 0).data(InstalledRole).toBool();
                if (!installed) foundUninstalled = true;
                if (foundUninstalled) QVERIFY(!installed);
            }
            const auto originalSize = library->gridSize(); density->setCurrentIndex(2);
            QVERIFY(library->gridSize().width() > originalSize.width());
        }
        Window reopened(data, cache, true);
        QCOMPARE(reopened.findChild<QComboBox *>("librarySort")->currentIndex(), 3);
        QCOMPARE(reopened.findChild<QComboBox *>("cardDensity")->currentIndex(), 2);
        QCOMPARE(reopened.findChild<QListView *>("libraryView")->gridSize(), QSize(192, 320));
    }
    void actionsFollowSelection() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        Window window(temp.path() + "/data", temp.path() + "/cache", true);
        auto *gallery = window.findChild<QListView *>("artGallery");
        auto *search = window.findChild<QLineEdit *>("librarySearch");
        QVERIFY(!action(window, "prepareArtwork")->isEnabled());
        QVERIFY(action(window, "viewCurrent")->isEnabled());
        gallery->setCurrentIndex(gallery->model()->index(0, 0));
        QVERIFY(action(window, "prepareArtwork")->isEnabled());
        search->setText("no-such-title");
        QVERIFY(!action(window, "prepareArtwork")->isEnabled());
        QVERIFY(!action(window, "viewCurrent")->isEnabled());
        QVERIFY(!action(window, "previewArtwork")->isEnabled());
        search->clear();
        QVERIFY(action(window, "viewCurrent")->isEnabled());
        QVERIFY(!action(window, "prepareArtwork")->isEnabled());
    }
    void responsiveEditorPreservesSelection() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        Window window(temp.path() + "/data", temp.path() + "/cache", true); window.show();
        window.resize(1024, 720); QCoreApplication::processEvents();
        QCOMPARE(window.width(), 1024);
        QVERIFY(window.findChildren<QStackedWidget *>().isEmpty()); QVERIFY(window.findChildren<QTabWidget *>().isEmpty());
        auto *scroll = window.findChild<QScrollArea *>("workspaceScroll"); QVERIFY(scroll); QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) { QTest::qWait(100); window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-compact.png"); }
        auto *gallery = window.findChild<QListView *>("artGallery"); gallery->setCurrentIndex(gallery->model()->index(0, 0));
        section(window, "queue"); QVERIFY(window.findChild<QWidget *>("artEditor")->isVisible()); section(window, "library");
        QVERIFY(gallery->currentIndex().isValid());
        QVERIFY(action(window, "prepareArtwork")->isEnabled());
        window.resize(1480, 940); QCoreApplication::processEvents();
        QVERIFY(window.findChild<QSplitter *>("librarySplit")->sizes().last() >= 390);
        if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) { QTest::qWait(100); window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-desktop.png"); }
    }
    void currentArtworkPreview() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        Window window(temp.path() + "/data", temp.path() + "/cache", true); window.show();
        action(window, "viewCurrent")->click();
        QVERIFY(!QApplication::activeModalWidget()); QVERIFY(window.findChild<QLabel *>("inlinePreviewTitle")->text().contains("muestra local")); QVERIFY(!window.findChild<QPushButton *>("prepareFromPreview")->isEnabled());
        QCOMPARE(window.findChild<QListWidget *>("queueList")->count(), 0);
        QVERIFY(!QFileInfo::exists(temp.path() + "/steam"));
    }
    void keyboardNavigation() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        Window window(temp.path() + "/data", temp.path() + "/cache", true); window.show(); window.activateWindow();
        QTest::qWait(30);
        QTest::keyClick(&window, Qt::Key_4, Qt::ControlModifier);
        QVERIFY(window.findChild<QScrollArea *>("workspaceScroll")->verticalScrollBar()->value() > 0);
        QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
        QVERIFY(window.findChild<QLineEdit *>("librarySearch")->hasFocus());
        auto *gallery = window.findChild<QListView *>("artGallery"); gallery->setCurrentIndex(gallery->model()->index(0, 0));
        QTest::keyClick(&window, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(window.findChild<QListWidget *>("queueList")->count(), 1);
    }
    void compareAndPrepareFromPreview() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        Window window(temp.path() + "/data", temp.path() + "/cache", true); window.show();
        auto *gallery = window.findChild<QListView *>("artGallery"); gallery->setCurrentIndex(gallery->model()->index(0, 0));
        action(window, "previewArtwork")->click();
        QVERIFY(!QApplication::activeModalWidget()); QCoreApplication::processEvents();
        auto *before = window.findChild<QWidget *>("comparisonBefore"); QVERIFY(before->isVisible()); QVERIFY(before->width() >= 300); QVERIFY(before->width() < window.width() - 300);
        window.findChild<QComboBox *>("previewBackground")->setCurrentIndex(2);
        if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-preview.png");
        window.findChild<QPushButton *>("prepareFromPreview")->click();
        QCOMPARE(window.findChild<QListWidget *>("queueList")->count(), 1);
        QVERIFY(!action(window, "applyQueue")->isEnabled());
        if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) {
            QTest::qWait(300);
            section(window, "queue"); window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-queue.png");
        }
    }
    void iconGalleryAndDraftPersist() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        const auto data = temp.path() + "/data", cache = temp.path() + "/cache";
        {
            Window window(data, cache, true); window.show();
            auto *types = window.findChild<QComboBox *>("artFormat"); QVERIFY(types); for (int i = 0; i < types->count(); ++i) QVERIFY(!types->itemIcon(i).isNull()); format(window, "icon"); QCOMPARE(types->currentText(), "Icono");
            auto *gallery = window.findChild<QListView *>("artGallery"); QCOMPARE(gallery->model()->rowCount(), 9);
            const auto art = gallery->model()->index(0, 0).data(ObjectRole).value<Artwork>();
            QCOMPARE(art.type, "icon"); QCOMPARE(art.width, art.height); QCOMPARE(art.width, 256); QVERIFY(!art.animated);
            QVERIFY(action(window, "viewCurrent")->isEnabled());
            for (auto *box : window.findChildren<QCheckBox *>()) if (box->text() == "Animadas") QVERIFY(!box->isEnabled());
            gallery->setCurrentIndex(gallery->model()->index(0, 0)); action(window, "prepareArtwork")->click();
            auto *queue = window.findChild<QListWidget *>("queueList"); QCOMPARE(queue->count(), 1); QVERIFY(queue->item(0)->text().contains("Icono")); QVERIFY(!queue->item(0)->icon().isNull());
            window.resize(1024, 720); QCoreApplication::processEvents(); QCOMPARE(window.width(), 1024);
            QCOMPARE(window.findChild<QScrollArea *>("workspaceScroll")->horizontalScrollBar()->maximum(), 0);
            if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) {
                QTest::qWait(400); window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/icons-compact.png");
                window.resize(1480, 940); QTest::qWait(400); window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/icons-desktop.png");
            }
        }
        Window reopened(data, cache, true); QCOMPARE(reopened.findChild<QListWidget *>("queueList")->count(), 1);
        QFile draft(data + "/draft.json"); QVERIFY(draft.open(QIODevice::ReadOnly)); QCOMPARE(QJsonDocument::fromJson(draft.readAll()).array().first().toObject().value("type").toString(), "icon");
        QVERIFY(!action(reopened, "applyQueue")->isEnabled());
    }
    void iconConfirmationRequiresAcknowledgement() {
        bool checked = false;
        QTimer::singleShot(0, [&checked] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); QVERIFY(box->checkBox());
            QVERIFY(!box->button(QMessageBox::Yes)->isEnabled());
            box->checkBox()->setChecked(true); QVERIFY(box->button(QMessageBox::Yes)->isEnabled()); checked = true;
            box->button(QMessageBox::No)->click();
        });
        QVERIFY(!Ui::confirmSteamChange(nullptr, "Prueba sintética", "No hay archivos de Steam en esta prueba.", true));
        QVERIFY(checked);
    }
    void iconComparisonCanPrepareWithoutApplying() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        Window window(temp.path() + "/data", temp.path() + "/cache", true); window.show();
        format(window, "icon");
        auto *gallery = window.findChild<QListView *>("artGallery"); gallery->setCurrentIndex(gallery->model()->index(0, 0));
        action(window, "previewArtwork")->click(); QVERIFY(!QApplication::activeModalWidget()); QVERIFY(window.findChild<QWidget *>("comparisonBefore")->isVisible());
        if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-icons-preview.png");
        window.findChild<QPushButton *>("prepareFromPreview")->click();
        auto *queue = window.findChild<QListWidget *>("queueList"); QCOMPARE(queue->count(), 1); QVERIFY(queue->item(0)->text().contains("Icono"));
        QVERIFY(!action(window, "applyQueue")->isEnabled());
    }
    void downloadsCanCancelAndRetryWithoutSteamWrites() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid()); FakeNetwork transport;
        QImage image(64, 64, QImage::Format_RGB32); image.fill(Qt::blue); QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer, "PNG"));
        FakeResponse slow; slow.body = png; slow.delay = 20; slow.chunk = 1; transport.responses.enqueue(slow); FakeResponse fast; fast.body = png; transport.responses.enqueue(fast);
        Window window(temp.path() + "/data", temp.path() + "/cache", true, nullptr, &transport); window.show();
        section(window, "downloads"); auto *view = window.findChild<QListView *>("downloadsView"); auto *tasks = qobject_cast<TransferModel *>(view->model()); QVERIFY(tasks); QVERIFY(!action(window, "cancelDownload")->isEnabled());
        const auto game = window.findChild<QListView *>("libraryView")->currentIndex().data(ObjectRole).value<Game>(); Artwork art; art.type = "capsule"; art.author = "Synthetic";
        QVERIFY(tasks->enqueue(QUrl("https://cdn2.steamgriddb.com/grid/synthetic-ui.png"), "Descarga de prueba sintética", {game, art, "stage"})); view->setCurrentIndex(tasks->index(0));
        QTRY_VERIFY(tasks->index(0).data(TransferModel::ReceivedRole).toLongLong() > 0);
        QVERIFY(action(window, "cancelDownload")->isEnabled());
        if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) {
            window.resize(1480, 940); section(window, "downloads"); window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-downloads.png");
            window.resize(1024, 720); QTest::qWait(50); section(window, "downloads"); window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-downloads-compact.png");
        }
        action(window, "cancelDownload")->click(); QCOMPARE(tasks->activeCount(), 0); QCOMPARE(window.findChild<QListWidget *>("queueList")->count(), 0);
        QVERIFY(action(window, "retryDownload")->isEnabled()); action(window, "retryDownload")->click();
        QTRY_COMPARE(window.findChild<QListWidget *>("queueList")->count(), 1); QVERIFY(!action(window, "applyQueue")->isEnabled());
    }
    void animatedPreviewStopsWhenHidden() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir temp("/tmp/opencode/studio-preview-XXXXXX"); QVERIFY(temp.isValid()); const auto gif = temp.path() + "/synthetic.gif";
        QProcess encoder; encoder.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "testsrc=size=32x48:rate=4:duration=1", "-loop", "0", "-y", gif}); QVERIFY(encoder.waitForFinished(10000)); QCOMPARE(encoder.exitCode(), 0);
        Preview preview; preview.show(); preview.setFile(gif, true); QSignalSpy playback(&preview, &Preview::playbackChanged); preview.togglePlayback(); QTRY_VERIFY(!playback.isEmpty() && playback.last().first().toBool()); preview.hide(); QVERIFY(!preview.playing()); QVERIFY(!playback.last().first().toBool());
    }
    void diagnosticPreviewDoesNotExportOrIncludePersonalData() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid());
        Window window(temp.path() + "/data", temp.path() + "/cache", true); window.show();
        action(window, "settings")->click();
        auto *contents = window.findChild<QTextEdit *>("diagnosticPreview"); QVERIFY(contents); QVERIFY(contents->isReadOnly()); QVERIFY(contents->toPlainText().isEmpty());
        window.findChild<QPushButton *>("exportDiagnostics")->click(); const auto text = contents->toPlainText(); QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(QJsonDocument::fromJson(text.toUtf8()).object().value("local_entries").toInt(), 15);
        QVERIFY(!text.contains(temp.path())); QVERIFY(!text.contains("Afterlight"));
        QVERIFY(!QFileInfo::exists(temp.path() + "/data/art-studio-diagnostico.json"));
    }
    void animationProfilesAreExplicitAndDoNotModifyDraftOrSource() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); QVERIFY(temp.isValid()); const auto source = temp.path() + "/synthetic.gif";
        QProcess encoder; encoder.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "testsrc=size=32x48:rate=4:duration=1", "-loop", "0", "-y", source}); QVERIFY(encoder.waitForFinished(10000)); QCOMPARE(encoder.exitCode(), 0);
        const auto data = temp.path() + "/data", cache = temp.path() + "/cache"; QDir().mkpath(data);
        CacheStore store(cache); const auto file = store.importFile(source); QVERIFY(!file.isEmpty());
        Game game; game.appId = 1; game.name = "Synthetic animation"; game.steamRoot = "demo"; game.userId = "0";
        QVERIFY(DraftStore(data, cache, true).save({{game, "capsule", file, "Synthetic", true}}));
        QFile draft(data + "/draft.json"); QVERIFY(draft.open(QIODevice::ReadOnly)); const auto before = draft.readAll(); draft.close();
        Window window(data, cache, true); window.show(); section(window, "queue");
        auto *profile = window.findChild<QComboBox *>("animationProfile"); QVERIFY(profile); QVERIFY(profile->isVisible()); QCOMPARE(profile->currentData().toInt(), int(AnimationProfile::Original));
        profile->setCurrentIndex(2); QCOMPARE(profile->currentData().toInt(), int(AnimationProfile::Small)); QVERIFY(!action(window, "applyQueue")->isEnabled());
        QVERIFY(draft.open(QIODevice::ReadOnly)); QCOMPARE(draft.readAll(), before); QVERIFY(QFileInfo(file).isFile());
        if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/animation-profiles.png");
    }
    void allFeaturesShareOneWorkspaceWithoutTabsOrDialogs() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); Window window(temp.path() + "/data", temp.path() + "/cache", true); window.show(); QCoreApplication::processEvents();
        auto *scroll = window.findChild<QScrollArea *>("workspaceScroll"); QVERIFY(scroll); QCOMPARE(scroll->widget()->objectName(), "unifiedWorkspace");
        for (const auto &name : {"library", "preview", "queue", "downloads", "backups", "settings"}) {
            auto *panel = window.findChild<QWidget *>(QString(name) + "Section"); QVERIFY(panel); QVERIFY(scroll->widget()->isAncestorOf(panel)); QVERIFY(panel->isVisible());
            section(window, name); QVERIFY(!QApplication::activeModalWidget());
        }
        QVERIFY(window.findChildren<QTabWidget *>().isEmpty()); QVERIFY(window.findChildren<QStackedWidget *>().isEmpty()); QVERIFY(window.findChildren<QDialog *>().isEmpty());
        QCOMPARE(window.findChild<QComboBox *>("artFormat")->count(), 5);
        QCOMPARE(window.findChild<QLineEdit *>("apiKeyInput")->echoMode(), QLineEdit::Password);
        QVERIFY(!action(window, "applyQueue")->isEnabled()); QVERIFY(!action(window, "restoreBackup")->isEnabled());
        if (!qEnvironmentVariableIsEmpty("STUDIO_SCREENSHOT_DIR")) {
            window.findChild<QPushButton *>("exportDiagnostics")->click(); section(window, "settings"); window.saveScreenshot(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-settings.png");
            QTest::qWait(300);
            scroll->widget()->grab().save(qEnvironmentVariable("STUDIO_SCREENSHOT_DIR") + "/workspace-complete.png");
        }
    }
    void switchingGameOrFormatClearsTheOldPreviewProposal() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); FakeNetwork transport; Window window(temp.path() + "/data", temp.path() + "/cache", true, nullptr, &transport); window.show();
        auto *gallery = window.findChild<QListView *>("artGallery"); gallery->setCurrentIndex(gallery->model()->index(0, 0)); action(window, "previewArtwork")->click();
        auto *prepare = window.findChild<QPushButton *>("prepareFromPreview"); QVERIFY(prepare->isEnabled());
        auto *games = window.findChild<QListView *>("libraryView"); games->setCurrentIndex(games->model()->index(1, 0)); QVERIFY(!prepare->isEnabled());
        gallery->setCurrentIndex(gallery->model()->index(0, 0)); action(window, "previewArtwork")->click(); QVERIFY(prepare->isEnabled());
        format(window, "hero"); QVERIFY(!prepare->isEnabled()); QCOMPARE(window.findChild<QListWidget *>("queueList")->count(), 0);
        QImage image(30, 45, QImage::Format_ARGB32); image.fill(Qt::blue); QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer, "PNG")); FakeResponse response; response.body = bytes; response.delay = 20; transport.responses.enqueue(response);
        const auto game = games->currentIndex().data(ObjectRole).value<Game>(); Artwork art; art.type = "hero";
        auto *tasks = qobject_cast<TransferModel *>(window.findChild<QListView *>("downloadsView")->model()); QVERIFY(tasks); QVERIFY(tasks->enqueue(QUrl("https://cdn2.steamgriddb.com/grid/synthetic-late-preview.png"), "Synthetic", {game, art, "preview"}));
        games->setCurrentIndex(games->model()->index(0, 0)); QTRY_COMPARE(tasks->activeCount(), 0); QVERIFY(!prepare->isEnabled()); QCOMPARE(window.findChild<QListWidget *>("queueList")->count(), 0); QCOMPARE(transport.requests.size(), 1);
    }
    void inlinePreviewPinsDownloadsAndQueueKeepsItsOwnOriginal() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); FakeNetwork transport; QImage image(40, 60, QImage::Format_ARGB32); image.fill(Qt::cyan); QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer, "PNG"));
        FakeResponse response; response.body = png; transport.responses.enqueue(response);
        Window window(temp.path() + "/data", temp.path() + "/cache", true, nullptr, &transport); window.show();
        auto *tasks = qobject_cast<TransferModel *>(window.findChild<QListView *>("downloadsView")->model()); QVERIFY(tasks);
        const auto game = window.findChild<QListView *>("libraryView")->currentIndex().data(ObjectRole).value<Game>(); Artwork art; art.type = "capsule";
        const QUrl url("https://cdn2.steamgriddb.com/grid/synthetic-inline.png"); QVERIFY(tasks->enqueue(url, "Synthetic", {game, art, "preview"}));
        auto *prepare = window.findChild<QPushButton *>("prepareFromPreview"); QTRY_VERIFY(prepare->isEnabled()); QVERIFY(!QApplication::activeModalWidget());
        CacheStore inspect(temp.path() + "/cache"); const auto original = inspect.lookup(url, 60).file; QVERIFY(!original.isEmpty()); prepare->click(); QCOMPARE(window.findChild<QListWidget *>("queueList")->count(), 1);
        auto clearCache = [&window] { QTimer::singleShot(0, [] { auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); box->button(QMessageBox::Yes)->click(); }); window.findChild<QPushButton *>("clearCache")->click(); };
        clearCache(); QVERIFY(QFileInfo::exists(original));
        window.findChild<QPushButton *>("clearPreview")->click(); clearCache(); QVERIFY(!QFileInfo::exists(original));
        QFile draft(temp.path() + "/data/draft.json"); QVERIFY(draft.open(QIODevice::ReadOnly)); const auto file = QJsonDocument::fromJson(draft.readAll()).array().first().toObject().value("file").toString(); QVERIFY(QFileInfo::exists(file)); QVERIFY(file != original); QCOMPARE(transport.requests.size(), 1);
    }
    void inlineAnimationStopsWhenScrolledOutOfView() {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) QSKIP("FFmpeg no disponible");
        QTemporaryDir temp("/tmp/opencode/studio-gui-XXXXXX"); const auto source = temp.path() + "/animation.gif";
        QProcess encoder; encoder.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "testsrc=size=32x48:rate=4:duration=1", "-loop", "0", "-y", source}); QVERIFY(encoder.waitForFinished(10000)); QCOMPARE(encoder.exitCode(), 0);
        const auto data = temp.path() + "/data", cache = temp.path() + "/cache"; QDir().mkpath(data); CacheStore store(cache); const auto file = store.importFile(source);
        Game game; game.appId = 1; game.name = "Synthetic animated preview"; game.steamRoot = "demo"; game.userId = "0"; QVERIFY(DraftStore(data, cache, true).save({{game, "capsule", file, "Synthetic", true}}));
        Window window(data, cache, true); window.show(); section(window, "queue"); auto *queue = window.findChild<QListWidget *>("queueList"); queue->setCurrentRow(0); queue->setFocus(); QTest::keyClick(queue, Qt::Key_Return);
        auto *preview = window.findChild<Preview *>("newPreview"); QVERIFY(preview->animated()); window.findChild<QPushButton *>("playInlineAnimation")->click(); QTRY_VERIFY(preview->playing());
        section(window, "settings"); QTRY_VERIFY(!preview->playing()); QVERIFY(!QApplication::activeModalWidget());
    }
    void unifiedWorkspaceCanConfirmApplyAndRestoreSyntheticSteamOnly() {
        QTemporaryDir temp("/tmp/opencode/studio-gui-transaction-XXXXXX"); QVERIFY(temp.isValid()); const auto steam = temp.path() + "/steam", data = temp.path() + "/data", cache = temp.path() + "/cache";
        auto write = [](const QString &path, const QByteArray &bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); if (!file.open(QIODevice::WriteOnly)) return false; return file.write(bytes) == bytes.size(); };
        auto read = [](const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); };
        QVERIFY(write(steam + "/config/loginusers.vdf", "\"users\" { \"76561197960265770\" { \"MostRecent\" \"1\" } }"));
        QVERIFY(write(steam + "/steamapps/appmanifest_100.acf", "\"AppState\" { \"appid\" \"100\" \"name\" \"Synthetic Game\" }"));
        const auto target = steam + "/userdata/42/config/grid/100p.png"; QDir().mkpath(QFileInfo(target).absolutePath()); QDir().mkpath(data);
        QImage image(32, 48, QImage::Format_ARGB32); image.fill(Qt::blue); QVERIFY(image.save(target)); const auto original = read(target);
        const auto source = temp.path() + "/new.png"; image.fill(Qt::cyan); QVERIFY(image.save(source)); CacheStore store(cache); const auto file = store.importFile(source);
        Game game; game.appId = 100; game.name = "Synthetic Game"; game.steamRoot = steam; game.userId = "42"; QVERIFY(DraftStore(data, cache).save({{game, "capsule", file, "Synthetic", false}}));
        FakeNetwork transport; Window window(data, cache, false, nullptr, &transport, {{steam, "Synthetic Steam only"}}); window.show();
        QTRY_COMPARE(window.findChild<QListView *>("libraryView")->model()->rowCount(), 1); QCOMPARE(read(target), original); QCOMPARE(transport.requests.size(), 0);
        QLabel *mode = nullptr; for (auto *label : window.findChildren<QLabel *>()) if (label->property("testId").toString() == "runModeNotice") mode = label;
        QVERIFY(mode); QVERIFY(mode->text().contains("MODO NORMAL")); QVERIFY(!mode->text().contains("DEMOSTRACIÓN"));
        QCOMPARE(window.findChild<QComboBox *>("animationProfile")->currentData().toInt(), int(AnimationProfile::Original));
        QVERIFY(action(window, "applyQueue")->isEnabled());
        auto answer = [&steam](QMessageBox::StandardButton button) {
            QTimer::singleShot(0, [steam, button] { auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); QVERIFY(box->text().contains(steam)); box->button(button)->click(); });
        };
        answer(QMessageBox::No); action(window, "applyQueue")->click(); QCOMPARE(read(target), original); QCOMPARE(window.findChild<QListWidget *>("queueList")->count(), 1);
        answer(QMessageBox::Yes); action(window, "applyQueue")->click(); QTRY_COMPARE(window.findChild<QListWidget *>("queueList")->count(), 0); QVERIFY(read(target) != original);
        auto *backups = window.findChild<QListWidget *>("backupList"); QTRY_COMPARE(backups->count(), 1); backups->setCurrentRow(0); QVERIFY(action(window, "restoreBackup")->isEnabled());
        answer(QMessageBox::Yes); action(window, "restoreBackup")->click(); QTRY_COMPARE(read(target), original); QTRY_COMPARE(backups->item(0)->data(Qt::UserRole + 1).toString(), QString("restored"));
        QVERIFY(!QApplication::activeModalWidget()); QVERIFY(!action(window, "restoreBackup")->isEnabled()); QCOMPARE(transport.requests.size(), 0);
    }
};
QTEST_MAIN(GuiTests)
#include "test_gui.moc"
