#pragma once
#include "cache.h"
#include "models.h"
#include "network.h"
#include "steam.h"
#include "transactions.h"
#include "ui.h"
#include "libraryindex.h"
#include "draft.h"
#include "transfers.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QScrollArea>
#include <QBoxLayout>
#include <QTextEdit>
#include <QTimer>

namespace Studio {
class Preview;
class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(const QString &dataDirectory, const QString &cacheDirectory, bool demo, QWidget *parent = nullptr, QNetworkAccessManager *transport = nullptr, const QVector<SteamRoot> &roots = {});
    ~Window() override;
    void saveScreenshot(const QString &path);
protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    QString m_dataDirectory;
    bool m_demo = false, m_busy = false, m_loadingArt = false, m_artFailed = false;
    CacheStore m_cache;
    Network m_network;
    ArtService m_art;
    TransferModel m_transfers;
    Transactions m_transactions;
    QSettings m_settings;
    ThumbnailLoader m_loader;
    CardModel m_libraryModel, m_artModel;
    QVector<SteamRoot> m_roots;
    QVector<Game> m_games;
    LibraryIndex m_libraryIndex;
    Game m_game;
    QVector<PendingChange> m_pending;
    QSet<QString> m_favorites;
    QJsonObject m_collections;
    QString m_activeCollection, m_type = "capsule", m_signature;
    QStringList m_watchedPaths;
    int m_page = 0, m_total = 0;
    struct ScanSnapshot { ScanResult scan; LibraryIndex index; };
    QFutureWatcher<ScanSnapshot> m_scanner;
    QFutureWatcher<TransactionResult> m_operation;
    QFileSystemWatcher m_watcher;
    QTimer m_poll, m_debounce;
    struct AssetRequest { QString purpose; Game game; Artwork art; int row = -1; quint64 revision = 0; };
    QHash<quint64, AssetRequest> m_assetRequests;
    CardView *m_libraryView = nullptr, *m_gallery = nullptr;
    CardDelegate *m_artDelegate = nullptr, *m_libraryDelegate = nullptr;
    QSplitter *m_split = nullptr;
    QWidget *m_editor = nullptr, *m_librarySection = nullptr;
    QScrollArea *m_workspaceScroll = nullptr;
    QBoxLayout *m_activityLayout = nullptr;
    QMap<QString, QWidget *> m_sections;
    QWidget *m_manualControls = nullptr;
    QString m_currentFile;
    QLineEdit *m_search = nullptr;
    QComboBox *m_rootChoice = nullptr, *m_filter = nullptr, *m_collectionChoice = nullptr;
    QComboBox *m_sort = nullptr, *m_density = nullptr, *m_typeChoice = nullptr;
    QCheckBox *m_favoritesOnly = nullptr;
    QLineEdit *m_collectionName = nullptr, *m_matchQuery = nullptr, *m_keyInput = nullptr;
    QComboBox *m_collectionTarget = nullptr, *m_matchResults = nullptr;
    QCheckBox *m_rememberKey = nullptr;
    QTextEdit *m_diagnosticContents = nullptr;
    Preview *m_inlineCurrent = nullptr, *m_inlineNew = nullptr;
    QLabel *m_inlineTitle = nullptr, *m_inlineInfo = nullptr;
    QPushButton *m_inlinePlay = nullptr, *m_inlinePrepare = nullptr;
    QString m_inlineFile, m_inlineCurrentFile;
    PendingChange m_previewProposal;
    QLabel *m_status = nullptr, *m_count = nullptr, *m_gameTitle = nullptr, *m_artStatus = nullptr, *m_pages = nullptr, *m_cacheLabel = nullptr;
    QLabel *m_heading = nullptr, *m_gameInfo = nullptr, *m_currentInfo = nullptr, *m_selectionInfo = nullptr;
    QLabel *m_queueSummary = nullptr, *m_queueEmpty = nullptr, *m_backupEmpty = nullptr;
    QLabel *m_typeNotice = nullptr, *m_statusIcon = nullptr;
    QLabel *m_draftNotice = nullptr;
    QLabel *m_operationNotice = nullptr;
    QWidget *m_animationControls = nullptr;
    QComboBox *m_animationProfile = nullptr;
    QSet<QString> m_draftBackupFiles;
    QCheckBox *m_static = nullptr, *m_animated = nullptr, *m_adult = nullptr;
    QPushButton *m_favorite = nullptr, *m_apply = nullptr, *m_restore = nullptr;
    QPushButton *m_stage = nullptr, *m_preview = nullptr, *m_local = nullptr, *m_currentButton = nullptr;
    QPushButton *m_previous = nullptr, *m_next = nullptr, *m_searchTitle = nullptr, *m_connectButton = nullptr;
    QPushButton *m_retryArt = nullptr;
    QProgressBar *m_progress = nullptr;
    QListWidget *m_queueList = nullptr, *m_backups = nullptr;
    QListView *m_downloadView = nullptr;
    QLabel *m_downloadEmpty = nullptr, *m_downloadDetails = nullptr;
    QProgressBar *m_downloadProgress = nullptr;
    QPushButton *m_cancelDownload = nullptr, *m_retryDownload = nullptr;
    void buildUi();
    void buildInlinePreview(QVBoxLayout *layout);
    void buildInlineSettings(QVBoxLayout *layout);
    void focusSection(const QString &name);
    void clearInlinePreview();
    void saveConnection();
    void updateRestoreAction();
    void updateActions();
    void updateCurrentArt();
    void updateLayout();
    void setCardDensity(int density);
    void scan(bool force = false);
    void checkChanges();
    void refilter();
    void selectGame(const QModelIndex &index);
    void fetchArt();
    void selectType(const QString &type);
    void connectService();
    void manualSearch();
    void localImage();
    void requestAsset(const QString &purpose);
    void assetReady(quint64 token, const QString &file, bool cached);
    void originalReady(const ArtTask &task, const QString &file, bool cached);
    void updateDownloads();
    void showPreview(const QString &file, bool animated, const QString &title, const PendingChange &proposal = {});
    bool stage(const Game &game, const QString &type, const QString &file, const QString &description, bool animated);
    void updateQueue();
    bool saveDraft();
    void syncDraftBackupPins();
    void loadDraft();
    void applyQueue();
    void refreshBackups();
    void restoreBackup();
    void createCollection();
    void addToCollection();
    void saveCollections();
    void demoLibrary();
    void message(const QString &text, bool error = false);
};
}
