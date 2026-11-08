#include "window.h"
#include "credentials.h"
#include "preview.h"
#include "imageutils.h"
#include <QApplication>
#include <QBuffer>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>
#include <QResizeEvent>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>

namespace Studio {
namespace {
QLabel *label(const QString &text, const QString &name = {}, QWidget *parent = nullptr) { auto w = new QLabel(text, parent); w->setTextFormat(Qt::PlainText); if (!name.isEmpty()) w->setObjectName(name); return w; }
QPushButton *button(const QString &text, const QString &name = {}, QWidget *parent = nullptr) { auto w = new QPushButton(text, parent); w->setCursor(Qt::PointingHandCursor); if (!name.isEmpty()) w->setObjectName(name); if (name == "primary") w->setProperty("intent", "primary"); return w; }
QString readableBytes(qint64 bytes) { return QString::number(bytes / 1024. / 1024., 'f', 1) + " MiB"; }
}
Window::Window(const QString &data, const QString &cache, bool demo, QWidget *parent, QNetworkAccessManager *transport, const QVector<SteamRoot> &roots)
    : QMainWindow(parent), m_dataDirectory(data), m_demo(demo), m_cache(cache), m_network(&m_cache, this, transport), m_art(&m_network, this), m_transfers(&m_network, this),
      m_transactions(data + "/backups"), m_settings(data + "/settings.ini", QSettings::IniFormat), m_loader(cache + "/posters", this),
      m_libraryModel(this), m_artModel(this) {
    QDir().mkpath(data); setWindowTitle("Steam Art Studio · " + QCoreApplication::applicationVersion() + " · Espacio único"); resize(1480, 940); setMinimumSize(1024, 720);
    setStyleSheet(Ui::stylesheet());
    const auto favorites = m_settings.value("library/favorites").toStringList(); for (const auto &key : favorites) m_favorites.insert(key);
    m_collections = QJsonDocument::fromJson(m_settings.value("library/collections").toByteArray()).object();
    buildUi(); loadDraft();
    connect(&m_transfers, &TransferModel::completed, this, &Window::originalReady);
    connect(&m_transfers, &TransferModel::changed, this, &Window::updateDownloads);
    connect(&m_transfers, &TransferModel::error, this, [this](const QString &text) { message(text, true); });
    auto repaint = new QTimer(this); repaint->setSingleShot(true); repaint->setInterval(16);
    connect(repaint, &QTimer::timeout, this, [this] { m_libraryView->viewport()->update(); m_gallery->viewport()->update(); });
    connect(&m_loader, &ThumbnailLoader::loaded, this, [repaint] { if (!repaint->isActive()) repaint->start(); });
    connect(&m_network, &Network::activity, this, [this](int active, int queued) {
        m_progress->setVisible(active > 0 || m_scanner.isRunning() || m_busy);
        if (active) m_status->setText(QString("%1 %2 · %3 en espera").arg(active).arg(active == 1 ? "descarga" : "descargas").arg(queued));
    });
    auto metrics = new QTimer(this);
    connect(metrics, &QTimer::timeout, this, [this] { m_cache.prune(); m_cacheLabel->setText("Caché " + readableBytes(m_cache.bytes()) + " / 512 MiB"); });
    metrics->start(5000);
    connect(&m_artModel, &CardModel::thumbnailNeeded, this, [this](int row, const QUrl &url, quint64 revision) {
        auto token = m_network.fetch(url); m_assetRequests[token] = {"thumb", {}, {}, row, revision};
    });
    connect(&m_network, &Network::ready, this, &Window::assetReady);
    connect(&m_network, &Network::failed, this, [this](quint64 token, const QString &error) {
        if (!m_assetRequests.contains(token)) return;
        const auto request = m_assetRequests.take(token); if (request.purpose != "thumb") message(error, true);
    });
    connect(&m_art, &ArtService::loading, this, [this](const QString &text) { m_loadingArt = true; m_artStatus->setText(text); m_gallery->setEmptyState("Buscando diseños…", "Puedes seguir explorando tu biblioteca mientras se completa la consulta."); updateActions(); });
    connect(&m_art, &ArtService::error, this, [this](const QString &text) { m_loadingArt = false; m_artStatus->setText(text); m_pages->setText("Sin resultados"); m_gallery->setEmptyState(m_network.hasApiKey() ? "No hay diseños disponibles" : "Descubre otro aspecto", m_network.hasApiKey() ? "Ajusta los filtros, busca otro título o utiliza una imagen local." : "Conecta SteamGridDB para explorar la comunidad, o prepara una imagen local sin conexión."); updateActions(); });
    connect(&m_art, &ArtService::results, this, [this](const QVector<Artwork> &art, int page, int total, bool cached) {
        m_loadingArt = false; m_artModel.setArtworks(art); m_page = page; m_total = total;
        m_pages->setText(QString("Página %1 · %2 diseños").arg(page + 1).arg(total));
        m_artStatus->setText(art.isEmpty() ? "No hay diseños con estos filtros. Puedes usar una imagen local." : cached ? "Desde tu caché · sin esperar otra descarga" : "Diseños de la comunidad · selecciona uno para prepararlo");
        m_gallery->setEmptyState("Sin diseños con estos filtros", "Prueba otro formato o activa imágenes estáticas o animadas."); updateActions();
    });
    connect(&m_scanner, &QFutureWatcher<ScanSnapshot>::finished, this, [this] {
        const auto snapshot = m_scanner.result(); const auto &result = snapshot.scan; m_progress->hide();
        if (!result.error.isEmpty()) { message(result.error, true); return; }
        if (m_game.appId && (m_game.steamRoot != result.root || m_game.userId != result.userId)) {
            m_game = Game(); m_art.invalidate(); m_artModel.setArtworks({});
        }
        m_games = result.games; m_libraryIndex = snapshot.index; m_watchedPaths = result.watchedPaths; m_signature = SteamScanner::signature(m_watchedPaths);
        for (const auto &game : m_games) if (game.key() == m_game.key()) { m_game = game; m_gameTitle->setText(game.name); break; }
        updateCurrentArt();
        if (!m_watcher.files().isEmpty()) m_watcher.removePaths(m_watcher.files());
        if (!m_watcher.directories().isEmpty()) m_watcher.removePaths(m_watcher.directories());
        QStringList watch;
        for (const auto &path : m_watchedPaths) { if (QFileInfo::exists(path)) watch.append(path); const auto parent = QFileInfo(path).absolutePath(); if (QFileInfo(parent).isDir()) watch.append(parent); }
        watch.removeDuplicates(); m_watcher.addPaths(watch); refilter();
        message(result.warnings.isEmpty() ? QString("Biblioteca actualizada · %1 juegos y aplicaciones locales").arg(m_games.size()) : result.warnings.join(' '));
    });
    m_debounce.setSingleShot(true); m_debounce.setInterval(1500);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_debounce, &QTimer::timeout, this, [this] { scan(true); });
    connect(&m_poll, &QTimer::timeout, this, &Window::checkChanges); m_poll.start(15000);
    connect(&m_operation, &QFutureWatcher<TransactionResult>::finished, this, [this] {
        const auto result = m_operation.result(); m_busy = false; m_progress->hide();
        if (result.ok) {
            for (const auto &change : m_pending) m_cache.unpin(change.file);
            m_pending.clear(); saveDraft(); updateQueue(); refreshBackups(); scan(true);
            m_operationNotice->hide(); message(QString("Se completaron %1 cambios. Steam puede necesitar recargar la biblioteca. No se ha reiniciado Steam.").arg(result.files));
        } else { message(result.error, true); m_operationNotice->setText("No se completó la operación: " + result.error); m_operationNotice->show(); refreshBackups(); }
        m_apply->setEnabled(!m_pending.isEmpty() && !m_demo);
        updateActions();
    });
    if (!demo) {
        m_roots = roots.isEmpty() ? SteamScanner::discover() : roots;
        { QSignalBlocker blocker(m_rootChoice); for (const auto &root : m_roots) m_rootChoice->addItem(root.label, root.path); }
        if (m_roots.isEmpty()) message("No se ha encontrado Steam. Instálalo e inicia sesión, o selecciona su carpeta desde Configuración.", true);
        else QTimer::singleShot(0, this, [this] { scan(true); });
        if (m_settings.value("wallet/enabled", false).toBool()) QTimer::singleShot(350, this, [this] { const auto key = Credentials::load(); if (!key.isEmpty()) { m_network.setApiKey(key); fetchArt(); } });
    } else { m_rootChoice->addItem("Demostración · sin Steam"); demoLibrary(); }
    if (m_settings.contains("window/geometry")) restoreGeometry(m_settings.value("window/geometry").toByteArray());
    if (m_settings.contains("view/workspaceSplitter")) m_split->restoreState(m_settings.value("view/workspaceSplitter").toByteArray());
    refreshBackups();
    updateLayout();
}
Window::~Window() { m_poll.stop(); m_debounce.stop(); clearInlinePreview(); m_scanner.waitForFinished(); m_operation.waitForFinished(); }
void Window::buildUi() {
    auto body = new QWidget; setCentralWidget(body); auto layout = new QVBoxLayout(body); layout->setContentsMargins(24, 18, 24, 14); layout->setSpacing(12);
    auto header = new QHBoxLayout; auto titles = new QVBoxLayout; titles->setSpacing(3); titles->addWidget(label("STEAM ART STUDIO / TODO EN UN ESPACIO", "eyebrow")); m_heading = label("Tu estudio de arte", "heading"); titles->addWidget(m_heading); header->addLayout(titles); header->addStretch();
    m_rootChoice = new QComboBox; m_rootChoice->setMinimumWidth(150); m_rootChoice->setMaximumWidth(245); m_rootChoice->setAccessibleName("Instalación de Steam"); header->addWidget(m_rootChoice);
    auto connectButton = button("Conectar SteamGridDB"); m_connectButton = connectButton; header->addWidget(connectButton); connect(connectButton, &QPushButton::clicked, this, &Window::connectService); layout->addLayout(header);
    connectButton->setIcon(Ui::icon(Ui::Symbol::Link));
    auto settings = button("Ajustes"); settings->setProperty("testId", "settings"); settings->setIcon(Ui::icon(Ui::Symbol::Settings)); header->addWidget(settings); connect(settings, &QPushButton::clicked, this, [this] { focusSection("settings"); });
    auto notice = label(m_demo ? "DEMOSTRACIÓN · Diseños ficticios. Aplicar cambios a Steam está desactivado." : "MODO NORMAL · 1  Elige un juego     /     2  Prepara sus imágenes     /     3  Revisa y confirma", "notice"); notice->setProperty("testId", "runModeNotice"); notice->setWordWrap(true); layout->addWidget(notice);
    m_draftNotice = label({}, "errorNotice"); m_draftNotice->setWordWrap(true); m_draftNotice->hide(); layout->addWidget(m_draftNotice);
    m_operationNotice = label({}, "errorNotice"); m_operationNotice->setWordWrap(true); m_operationNotice->hide(); layout->addWidget(m_operationNotice);
    m_workspaceScroll = new QScrollArea; m_workspaceScroll->setObjectName("workspaceScroll"); m_workspaceScroll->setWidgetResizable(true); m_workspaceScroll->setFrameShape(QFrame::NoFrame); m_workspaceScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto workspace = new QWidget; workspace->setObjectName("unifiedWorkspace"); auto workspaceLayout = new QVBoxLayout(workspace); workspaceLayout->setContentsMargins(0, 0, 8, 0); workspaceLayout->setSpacing(22); m_workspaceScroll->setWidget(workspace); layout->addWidget(m_workspaceScroll, 1);
    auto library = new QWidget; m_librarySection = library; library->setObjectName("librarySection"); m_sections["library"] = library; auto libLayout = new QVBoxLayout(library); libLayout->setContentsMargins(0, 0, 0, 0); libLayout->setSpacing(10);
    auto toolbar = new QHBoxLayout; m_search = new QLineEdit; m_search->setPlaceholderText("Buscar juegos…  Ctrl+F"); m_search->setAccessibleName("Buscar en la biblioteca"); m_search->setClearButtonEnabled(true); m_search->addAction(Ui::icon(Ui::Symbol::Search), QLineEdit::LeadingPosition); toolbar->addWidget(m_search, 1);
    m_filter = new QComboBox; m_filter->setObjectName("libraryFilter"); m_filter->setAccessibleName("Filtrar juegos"); m_filter->addItems({"Todos", "Instalados", "Externos", "Recientes"}); toolbar->addWidget(m_filter);
    m_collectionChoice = new QComboBox; m_collectionChoice->setObjectName("collectionFilter"); m_collectionChoice->setAccessibleName("Colección local"); m_collectionChoice->setMinimumContentsLength(12); m_collectionChoice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); m_collectionChoice->setMinimumWidth(190); m_collectionChoice->setMaximumWidth(220); m_collectionChoice->addItem("Todas las colecciones"); for (auto it = m_collections.begin(); it != m_collections.end(); ++it) m_collectionChoice->addItem(it.key()); toolbar->addWidget(m_collectionChoice);
    m_favoritesOnly = new QCheckBox("Solo favoritos"); m_favoritesOnly->setObjectName("favoritesFilter"); toolbar->addWidget(m_favoritesOnly); connect(m_favoritesOnly, &QCheckBox::toggled, this, [this] { refilter(); });
    auto refresh = button(""); refresh->setIcon(Ui::icon(Ui::Symbol::Refresh)); refresh->setAccessibleName("Actualizar biblioteca"); refresh->setToolTip("Actualizar biblioteca local · F5"); toolbar->addWidget(refresh);
    libLayout->addLayout(toolbar);
    auto collectionRow = new QHBoxLayout; collectionRow->addWidget(label("Colecciones locales", "muted")); m_collectionName = new QLineEdit; m_collectionName->setObjectName("newCollectionName"); m_collectionName->setPlaceholderText("Nombre de una nueva colección"); m_collectionName->setMaxLength(60); collectionRow->addWidget(m_collectionName, 1);
    auto newCollection = button("Crear colección"); newCollection->setIcon(Ui::icon(Ui::Symbol::Plus)); newCollection->setProperty("testId", "newCollection"); collectionRow->addWidget(newCollection); connect(newCollection, &QPushButton::clicked, this, &Window::createCollection); connect(m_collectionName, &QLineEdit::returnPressed, this, &Window::createCollection); libLayout->addLayout(collectionRow);
    auto split = new QSplitter; m_split = split; split->setObjectName("librarySplit"); split->setHandleWidth(5); libLayout->addWidget(split, 1);
    auto games = new QWidget; auto gamesLayout = new QVBoxLayout(games); gamesLayout->setContentsMargins(0, 0, 12, 0); gamesLayout->setSpacing(8);
    auto gameHeading = new QHBoxLayout; m_count = label("Detectando…", "muted"); gameHeading->addWidget(m_count); gameHeading->addStretch();
    m_sort = new QComboBox; m_sort->setObjectName("librarySort"); m_sort->setAccessibleName("Ordenar juegos"); m_sort->addItems({"Nombre A–Z", "Nombre Z–A", "Última sesión", "Instalados primero"}); gameHeading->addWidget(m_sort);
    m_density = new QComboBox; m_density->setObjectName("cardDensity"); m_density->setAccessibleName("Tamaño de las tarjetas"); m_density->addItems({"Compactas", "Medianas", "Grandes"}); gameHeading->addWidget(m_density); gamesLayout->addLayout(gameHeading);
    m_libraryView = new CardView; m_libraryView->setAccessibleName("Juegos de tu biblioteca"); m_libraryView->setModel(&m_libraryModel); m_libraryView->setWordWrap(true);
    m_libraryDelegate = new CardDelegate(&m_loader, false, m_libraryView); m_libraryView->setItemDelegate(m_libraryDelegate); gamesLayout->addWidget(m_libraryView, 1); split->addWidget(games);
    games->setMinimumWidth(310);
    auto detail = new QWidget; m_editor = detail; detail->setObjectName("artEditor"); detail->setMinimumWidth(390); auto detailLayout = new QVBoxLayout(detail); detailLayout->setContentsMargins(16, 0, 0, 0); detailLayout->setSpacing(8);
    auto gameTop = new QHBoxLayout; m_gameTitle = label("Selecciona un juego", "gameTitle"); m_gameTitle->setWordWrap(true); gameTop->addWidget(m_gameTitle, 1);
    m_favorite = button(""); m_favorite->setIcon(Ui::icon(Ui::Symbol::Star)); m_favorite->setCheckable(true); m_favorite->setAccessibleName("Favorito"); m_favorite->setProperty("testId", "toggleFavorite"); m_favorite->setToolTip("Marcar como favorito en esta aplicación"); gameTop->addWidget(m_favorite); detailLayout->addLayout(gameTop);
    m_collectionTarget = new QComboBox; m_collectionTarget->setObjectName("collectionTarget"); m_collectionTarget->setAccessibleName("Colección para el juego seleccionado"); m_collectionTarget->setMinimumContentsLength(12); m_collectionTarget->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); m_collectionTarget->addItem("Elige una colección", QString()); for (auto it = m_collections.begin(); it != m_collections.end(); ++it) m_collectionTarget->addItem(it.key(), it.key()); m_collectionTarget->setMaximumWidth(220); collectionRow->addWidget(m_collectionTarget);
    auto collection = button("Añadir juego"); collection->setIcon(Ui::icon(Ui::Symbol::Collection)); collection->setProperty("testId", "addToCollection"); collectionRow->addWidget(collection);
    m_gameInfo = label("Elige un juego para empezar", "muted"); m_gameInfo->setWordWrap(true); detailLayout->addWidget(m_gameInfo);
    auto formatRow = new QHBoxLayout; formatRow->addWidget(label("Formato", "muted")); m_typeChoice = new QComboBox; m_typeChoice->setObjectName("artFormat"); m_typeChoice->setAccessibleName("Formato de imagen"); for (const auto &type : artTypes()) m_typeChoice->addItem(Ui::icon(Ui::artSymbol(type)), artTitle(type), type); formatRow->addWidget(m_typeChoice, 1); detailLayout->addLayout(formatRow); connect(m_typeChoice, &QComboBox::currentIndexChanged, this, [this] { selectType(m_typeChoice->currentData().toString()); });
    m_currentButton = button("Ver actual"); m_currentButton->setProperty("testId", "viewCurrent"); m_currentButton->setIcon(Ui::icon(Ui::Symbol::Preview)); formatRow->addWidget(m_currentButton);
    m_typeNotice = label({}, "typeNotice"); m_typeNotice->setWordWrap(true); m_typeNotice->setVisible(false); detailLayout->addWidget(m_typeNotice);
    auto filters = new QHBoxLayout; m_static = new QCheckBox("Estáticas"); m_static->setChecked(true); m_animated = new QCheckBox("Animadas"); m_animated->setChecked(true); m_adult = new QCheckBox("Adultos");
    filters->addWidget(m_static); filters->addWidget(m_animated); filters->addStretch(); filters->addWidget(m_adult); detailLayout->addLayout(filters);
    m_artStatus = label("Las carátulas actuales se leen de Steam. Conecta SteamGridDB para explorar diseños nuevos.", "muted"); m_artStatus->setWordWrap(true); detailLayout->addWidget(m_artStatus);
    m_gallery = new CardView; m_gallery->setObjectName("artGallery"); m_gallery->setAccessibleName("Diseños disponibles"); m_gallery->setModel(&m_artModel); m_gallery->setGridSize(QSize(148, 250));
    m_artDelegate = new CardDelegate(&m_loader, true, m_gallery); m_gallery->setItemDelegate(m_artDelegate); detailLayout->addWidget(m_gallery, 1);
    auto pages = new QHBoxLayout; auto previous = button(""); auto next = button(""); m_previous = previous; m_next = next; previous->setIcon(Ui::icon(Ui::Symbol::Left)); next->setIcon(Ui::icon(Ui::Symbol::Right)); previous->setAccessibleName("Página anterior"); next->setAccessibleName("Página siguiente"); m_pages = label("Sin selección", "muted"); pages->addWidget(previous); pages->addWidget(m_pages, 1, Qt::AlignCenter); pages->addWidget(next);
    detailLayout->addLayout(pages);
    m_manualControls = new QWidget; auto manualLayout = new QVBoxLayout(m_manualControls); manualLayout->setContentsMargins(0, 0, 0, 0); manualLayout->addWidget(label("Asociar el juego seleccionado con otro título de SteamGridDB (solo en Art Studio)", "muted"));
    auto matchRow = new QHBoxLayout; m_matchQuery = new QLineEdit; m_matchQuery->setObjectName("manualTitle"); m_matchQuery->setPlaceholderText("Buscar otro título en SteamGridDB"); m_matchQuery->setMaxLength(200); matchRow->addWidget(m_matchQuery, 1); auto searchTitle = button("Buscar título"); m_searchTitle = searchTitle; matchRow->addWidget(searchTitle); connect(searchTitle, &QPushButton::clicked, this, &Window::manualSearch); connect(m_matchQuery, &QLineEdit::returnPressed, this, &Window::manualSearch); manualLayout->addLayout(matchRow);
    auto matchChoice = new QHBoxLayout; m_matchResults = new QComboBox; m_matchResults->setObjectName("titleMatches"); m_matchResults->setPlaceholderText("Coincidencias de SteamGridDB"); matchChoice->addWidget(m_matchResults, 1); auto associate = button("Asociar título"); associate->setObjectName("associateTitle"); matchChoice->addWidget(associate); manualLayout->addLayout(matchChoice); connect(associate, &QPushButton::clicked, this, [this] { const auto id = m_matchResults->currentData().toLongLong(); if (!m_game.appId || id <= 0 || m_busy) return; auto mappings = m_settings.value("sgdb/mappings").toMap(); mappings[m_game.key()] = id; m_settings.setValue("sgdb/mappings", mappings); m_art.setMapping(m_game.key(), id); m_page = 0; fetchArt(); });
    m_selectionInfo = label("Selecciona un diseño para prepararlo", "selectionInfo"); m_selectionInfo->setWordWrap(true); detailLayout->addWidget(m_selectionInfo);
    auto actions = new QHBoxLayout; auto local = button("Imagen local…"); m_local = local; auto preview = button("Vista previa"); m_preview = preview; auto stage = button("Preparar", "primary"); m_stage = stage; stage->setIcon(Ui::icon(Ui::Symbol::Plus, QColor("#123f33"))); stage->setToolTip("Preparar este diseño · Ctrl+Intro"); stage->setProperty("testId", "prepareArtwork"); preview->setProperty("testId", "previewArtwork"); actions->addWidget(local); actions->addWidget(preview); actions->addWidget(stage); detailLayout->addLayout(actions);
    local->setIcon(Ui::icon(Ui::Symbol::Image)); preview->setIcon(Ui::icon(Ui::Symbol::Preview));
    split->addWidget(detail); split->setSizes({600, 700}); split->setChildrenCollapsible(false); workspaceLayout->addWidget(library);
    buildInlinePreview(workspaceLayout);
    auto queue = new QWidget; queue->setObjectName("queueSection"); m_sections["queue"] = queue; auto queueLayout = new QVBoxLayout(queue); queueLayout->setContentsMargins(0, 0, 0, 0); queueLayout->addWidget(label("Cambios preparados", "sectionTitle"));
    auto queueInfo = label("Nada se escribe hasta que pulses Aplicar y confirmes. Cada lote guarda una copia restaurable de las imágenes anteriores.", "notice"); queueInfo->setWordWrap(true); queueLayout->addWidget(queueInfo);
    m_queueSummary = label("Sin cambios preparados", "queueSummary"); queueLayout->addWidget(m_queueSummary);
    m_animationControls = new QWidget; auto animationLayout = new QVBoxLayout(m_animationControls); animationLayout->setContentsMargins(0, 0, 0, 0);
    auto animationRow = new QHBoxLayout; animationRow->addWidget(label("Animaciones al aplicar", "muted")); m_animationProfile = new QComboBox; m_animationProfile->setObjectName("animationProfile"); m_animationProfile->setAccessibleName("Calidad de las animaciones al aplicar");
    for (const auto profile : {AnimationProfile::Original, AnimationProfile::Balanced, AnimationProfile::Small}) m_animationProfile->addItem(animationProfileTitle(profile), static_cast<int>(profile));
    m_animationProfile->setCurrentIndex(m_animationProfile->findData(static_cast<int>(AnimationProfile::Original)));
    animationRow->addWidget(m_animationProfile, 1); animationLayout->addLayout(animationRow);
    auto animationHelp = label("Original conserva la resolución y los fotogramas, sin reducción automática de calidad. Equilibrada/Ligera solo reducen resolución/FPS si las eliges. El APNG puede ocupar más que el WebP/GIF: límite de 64 MiB y 120 s por conversión; si se supera, se cancela sin bajar calidad ni modificar el original o la cola.", "muted"); animationHelp->setWordWrap(true); animationLayout->addWidget(animationHelp); queueLayout->addWidget(m_animationControls);
    m_queueEmpty = label("Tu cola está vacía. Selecciona un diseño arriba y pulsa Preparar; nada se aplica automáticamente.", "muted"); m_queueEmpty->setWordWrap(true); queueLayout->addWidget(m_queueEmpty);
    m_queueList = new QListWidget; m_queueList->setObjectName("queueList"); queueLayout->addWidget(m_queueList); auto queueActions = new QHBoxLayout; auto remove = button("Quitar seleccionado"); remove->setProperty("testId", "removeQueued"); auto clear = button("Vaciar cola"); clear->setProperty("testId", "clearQueue"); m_apply = button("Aplicar cambios…", "primary"); m_apply->setProperty("testId", "applyQueue"); queueActions->addWidget(remove); queueActions->addWidget(clear); queueActions->addStretch(); queueActions->addWidget(m_apply); queueLayout->addLayout(queueActions); workspaceLayout->addWidget(queue);
    remove->setIcon(Ui::icon(Ui::Symbol::Close)); clear->setIcon(Ui::icon(Ui::Symbol::Trash)); m_apply->setIcon(Ui::icon(Ui::Symbol::Check, QColor("#123f33")));
    auto backups = new QWidget; backups->setObjectName("backupsSection"); m_sections["backups"] = backups; auto backupsLayout = new QVBoxLayout(backups); backupsLayout->setContentsMargins(0, 0, 0, 0); backupsLayout->addWidget(label("Copias y restauración", "sectionTitle")); auto backupInfo = label("Restauración confirmada con comprobación de conflictos. Las copias no se borran al limpiar la caché.", "muted"); backupInfo->setWordWrap(true); backupsLayout->addWidget(backupInfo);
    m_backupEmpty = label("Aún no hay copias. Se crean al aplicar un lote, nunca al explorar o preparar.", "muted"); m_backupEmpty->setWordWrap(true); backupsLayout->addWidget(m_backupEmpty);
    m_backups = new QListWidget; m_backups->setObjectName("backupList"); m_backups->setAccessibleName("Historial de copias"); backupsLayout->addWidget(m_backups); auto backupActions = new QHBoxLayout; auto open = button("Abrir copias"); open->setIcon(Ui::icon(Ui::Symbol::Collection)); auto restore = button("Restaurar…"); m_restore = restore; restore->setProperty("testId", "restoreBackup"); restore->setEnabled(false); backupActions->addWidget(open); backupActions->addStretch(); backupActions->addWidget(restore); backupsLayout->addLayout(backupActions);
    restore->setIcon(Ui::icon(Ui::Symbol::Backup, QColor("#123f33")));
    connect(m_backups, &QListWidget::currentItemChanged, this, [this] { updateRestoreAction(); });
    auto downloads = new QWidget; downloads->setObjectName("downloadsSection"); m_sections["downloads"] = downloads; auto downloadsLayout = new QVBoxLayout(downloads); downloadsLayout->setContentsMargins(0, 0, 0, 0); downloadsLayout->addWidget(label("Descargas", "sectionTitle"));
    auto downloadInfo = label("Originales para vista previa o cola. Cancelar no modifica Steam. Las miniaturas son automáticas.", "muted"); downloadInfo->setWordWrap(true); downloadsLayout->addWidget(downloadInfo);
    m_downloadEmpty = label("No hay descargas. Previsualiza o prepara un diseño para solicitar su original.", "muted"); m_downloadEmpty->setWordWrap(true); downloadsLayout->addWidget(m_downloadEmpty);
    m_downloadView = new QListView; m_downloadView->setObjectName("downloadsView"); m_downloadView->setAccessibleName("Descargas de originales"); m_downloadView->setModel(&m_transfers); m_downloadView->setWordWrap(true); downloadsLayout->addWidget(m_downloadView, 1);
    m_downloadDetails = label("Selecciona una descarga", "muted"); m_downloadDetails->setWordWrap(true); downloadsLayout->addWidget(m_downloadDetails);
    m_downloadProgress = new QProgressBar; m_downloadProgress->setRange(0, 100); m_downloadProgress->setTextVisible(false); downloadsLayout->addWidget(m_downloadProgress);
    auto downloadActions = new QHBoxLayout; m_cancelDownload = button("Cancelar seleccionada"); m_cancelDownload->setIcon(Ui::icon(Ui::Symbol::Close)); m_cancelDownload->setProperty("testId", "cancelDownload");
    m_retryDownload = button("Repetir / reintentar"); m_retryDownload->setIcon(Ui::icon(Ui::Symbol::Refresh)); m_retryDownload->setProperty("testId", "retryDownload");
    m_cancelDownload->setText("Cancelar"); m_retryDownload->setText("Reintentar"); auto cleanDownloads = button("Quitar terminadas"); downloadActions->addWidget(m_cancelDownload); downloadActions->addWidget(m_retryDownload); downloadActions->addStretch(); downloadActions->addWidget(cleanDownloads); downloadsLayout->addLayout(downloadActions);
    auto activity = new QWidget; m_activityLayout = new QBoxLayout(QBoxLayout::LeftToRight, activity); m_activityLayout->setContentsMargins(0, 0, 0, 0); m_activityLayout->setSpacing(24); m_activityLayout->addWidget(downloads, 1); m_activityLayout->addWidget(backups, 1); workspaceLayout->addWidget(activity);
    connect(m_downloadView->selectionModel(), &QItemSelectionModel::currentChanged, this, &Window::updateDownloads);
    connect(m_cancelDownload, &QPushButton::clicked, this, [this] { m_transfers.cancel(m_downloadView->currentIndex().row()); });
    connect(m_retryDownload, &QPushButton::clicked, this, [this] { if (!m_busy) m_transfers.retry(m_downloadView->currentIndex().row()); });
    connect(cleanDownloads, &QPushButton::clicked, &m_transfers, &TransferModel::clearFinished);
    buildInlineSettings(workspaceLayout); workspaceLayout->addStretch();
    m_progress = new QProgressBar; m_progress->setRange(0, 0); m_progress->setTextVisible(false); m_progress->hide(); layout->addWidget(m_progress);
    auto statusRow = new QHBoxLayout; m_statusIcon = new QLabel; m_statusIcon->setFixedSize(18, 18); m_statusIcon->setPixmap(Ui::icon(Ui::Symbol::Refresh).pixmap(18, 18)); statusRow->addWidget(m_statusIcon); m_status = label("Preparando biblioteca local…", "muted"); m_status->setWordWrap(true); statusRow->addWidget(m_status, 1); layout->addLayout(statusRow);
    connect(m_search, &QLineEdit::textChanged, this, [this] { refilter(); }); connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { refilter(); });
    m_search->setObjectName("librarySearch"); m_libraryView->setObjectName("libraryView");
    m_sort->setCurrentIndex(qBound(0, m_settings.value("view/sort", 0).toInt(), 3));
    m_density->setCurrentIndex(qBound(0, m_settings.value("view/density", 1).toInt(), 2));
    setCardDensity(m_density->currentIndex());
    connect(m_sort, &QComboBox::currentIndexChanged, this, [this](int index) { m_settings.setValue("view/sort", index); refilter(); });
    connect(m_density, &QComboBox::currentIndexChanged, this, &Window::setCardDensity);
    connect(m_currentButton, &QPushButton::clicked, this, [this] { if (!m_currentFile.isEmpty()) showPreview(m_currentFile, false, m_game.name + (m_demo ? " · muestra local" : " · actual en Steam")); });
    connect(m_gallery->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { updateActions(); });
    connect(&m_artModel, &QAbstractItemModel::modelReset, this, &Window::updateActions);
    connect(m_collectionChoice, &QComboBox::currentTextChanged, this, [this](const QString &text) { m_activeCollection = text == "Todas las colecciones" ? QString() : text; refilter(); });
    connect(refresh, &QPushButton::clicked, this, [this] { scan(true); }); connect(m_rootChoice, &QComboBox::currentIndexChanged, this, [this] { scan(true); });
    connect(m_libraryView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) { if (index.isValid()) selectGame(index); });
    connect(m_libraryView, &QListView::doubleClicked, this, [this](const QModelIndex &index) { auto game = m_libraryModel.game(index.row()); if (!game.cover.isEmpty()) showPreview(game.cover, false, game.name + " · carátula actual"); });
    m_libraryView->setContextMenuPolicy(Qt::CustomContextMenu); connect(m_libraryView, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        auto index = m_libraryView->indexAt(point); if (!index.isValid()) return; m_libraryView->setCurrentIndex(index);
        QMenu menu; auto fav = menu.addAction("Alternar favorito"); auto add = menu.addAction("Añadir a colección…");
        QAction *remove = nullptr;
        if (!m_activeCollection.isEmpty()) remove = menu.addAction("Quitar de esta colección");
        auto chosen = menu.exec(m_libraryView->viewport()->mapToGlobal(point));
        if (chosen == fav) m_favorite->click();
        if (chosen == add) addToCollection();
        if (remove && chosen == remove) {
            QJsonArray entries;
            for (const auto &entry : m_collections.value(m_activeCollection).toArray()) if (entry.toString() != m_game.key()) entries.append(entry);
            m_collections[m_activeCollection] = entries; saveCollections(); refilter();
        }
    });
    connect(m_favorite, &QPushButton::clicked, this, [this] { if (!m_game.appId) return; auto key = m_game.key(); if (m_favorites.contains(key)) m_favorites.remove(key); else m_favorites.insert(key); saveCollections(); refilter(); updateActions(); });
    connect(collection, &QPushButton::clicked, this, &Window::addToCollection);
    connect(m_collectionTarget, &QComboBox::currentIndexChanged, this, [this] { updateActions(); });
    connect(m_queueList, &QListWidget::currentRowChanged, this, [this] { updateActions(); });
    m_artStatus->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_artStatus, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QMenu menu; auto search = menu.addAction("Buscar título en SteamGridDB…");
        if (menu.exec(m_artStatus->mapToGlobal(point)) == search) manualSearch();
    });
    for (auto checkbox : {m_static, m_animated, m_adult}) connect(checkbox, &QCheckBox::toggled, this, [this] { m_page = 0; fetchArt(); });
    connect(previous, &QPushButton::clicked, this, [this] { if (m_page > 0) { --m_page; fetchArt(); } }); connect(next, &QPushButton::clicked, this, [this] { if ((m_page + 1) * 50 < m_total) { ++m_page; fetchArt(); } });
    connect(local, &QPushButton::clicked, this, &Window::localImage); connect(preview, &QPushButton::clicked, this, [this] { requestAsset("preview"); }); connect(stage, &QPushButton::clicked, this, [this] { requestAsset("stage"); });
    connect(m_gallery, &QListView::activated, this, [this] { if (m_preview->isEnabled()) requestAsset("preview"); });
    m_gallery->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gallery, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QMenu menu; auto search = menu.addAction("Buscar título…");
        if (menu.exec(m_gallery->viewport()->mapToGlobal(point)) == search) manualSearch();
    });
    connect(m_queueList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        const int row = m_queueList->row(item);
        if (row >= 0 && row < m_pending.size()) { const auto change = m_pending.at(row); showPreview(change.file, change.animated, change.game.name + " · " + artTitle(change.type)); }
    });
    connect(remove, &QPushButton::clicked, this, [this] { if (m_busy) return; auto row = m_queueList->currentRow(); if (row >= 0 && row < m_pending.size()) { m_cache.unpin(m_pending.at(row).file); m_pending.removeAt(row); saveDraft(); updateQueue(); } });
    connect(clear, &QPushButton::clicked, this, [this] { if (m_busy || m_pending.isEmpty()) return; if (QMessageBox::question(this, "Vaciar cola", "¿Descartar los cambios preparados? Las imágenes de Steam no se tocarán.") != QMessageBox::Yes) return; for (const auto &change : m_pending) m_cache.unpin(change.file); m_pending.clear(); saveDraft(); updateQueue(); });
    connect(m_apply, &QPushButton::clicked, this, &Window::applyQueue); connect(restore, &QPushButton::clicked, this, &Window::restoreBackup);
    connect(open, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_transactions.root())); });
    auto searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, this, [this] { focusSection("library"); m_search->setFocus(); m_search->selectAll(); });
    auto refreshShortcut = new QShortcut(QKeySequence("F5"), this); connect(refreshShortcut, &QShortcut::activated, this, [this] { scan(true); });
    auto prepareShortcut = new QShortcut(QKeySequence("Ctrl+Return"), this); connect(prepareShortcut, &QShortcut::activated, this, [this] { if (m_inlinePrepare->hasFocus() && m_inlinePrepare->isEnabled()) m_inlinePrepare->click(); else if (m_stage->isEnabled()) m_stage->click(); });
    const QStringList sections = {"library", "library", "library", "queue", "backups", "downloads"};
    for (int i = 0; i < sections.size(); ++i) {
        auto shortcut = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)), this);
        connect(shortcut, &QShortcut::activated, this, [this, name = sections.at(i), i] { focusSection(name); if (i == 1) m_favoritesOnly->setChecked(!m_favoritesOnly->isChecked()); if (i == 2) m_collectionChoice->setFocus(); });
    }
    updateActions(); updateDownloads();
}
void Window::updateDownloads() {
    if (!m_downloadView) return;
    const QModelIndex selected = m_downloadView->currentIndex(); const bool valid = selected.isValid();
    const int state = selected.data(TransferModel::StateRole).toInt(); const bool active = valid && (state == TransferModel::Waiting || state == TransferModel::Downloading);
    m_cancelDownload->setEnabled(active); m_retryDownload->setEnabled(valid && !active && !m_busy);
    m_downloadEmpty->setVisible(m_transfers.rowCount() == 0); m_downloadView->setVisible(m_transfers.rowCount() > 0);
    m_downloadDetails->setText(valid ? selected.data().toString() : "Selecciona una descarga. Repetir descarga no aplica imágenes a Steam.");
    m_downloadProgress->setVisible(active);
    const qint64 received = selected.data(TransferModel::ReceivedRole).toLongLong(), total = selected.data(TransferModel::TotalRole).toLongLong();
    m_downloadProgress->setRange(0, total > 0 ? 100 : 0); if (total > 0) m_downloadProgress->setValue(qBound<qint64>(0LL, received * 100 / total, 100LL));
    m_downloadView->setFixedHeight(qBound(80, m_transfers.rowCount() * 80, 240));
    updateLayout();
}
void Window::updateActions() {
    if (!m_stage) return;
    if (m_animationProfile) m_animationProfile->setEnabled(!m_busy);
    if (m_inlinePrepare) m_inlinePrepare->setEnabled(m_previewProposal.game.appId && !m_busy);
    if (m_restore) updateRestoreAction();
    const bool selected = m_game.appId != 0;
    const bool artwork = m_gallery->currentIndex().isValid();
    const bool iconTarget = m_type != "icon" || m_demo || !Transactions::destination(m_game, m_type).isEmpty();
    m_stage->setEnabled(selected && artwork && !m_busy && !m_loadingArt && iconTarget);
    m_stage->setToolTip(iconTarget ? "Preparar este diseño · Ctrl+Intro" : "No se conoce el identificador local del icono. Actualiza la biblioteca antes de prepararlo.");
    m_preview->setEnabled(selected && (artwork || !m_currentFile.isEmpty()));
    m_local->setEnabled(selected && !m_busy);
    m_currentButton->setEnabled(selected && !m_currentFile.isEmpty());
    m_searchTitle->setEnabled(selected && !m_demo && !m_busy);
    m_previous->setEnabled(!m_demo && !m_loadingArt && m_page > 0);
    m_next->setEnabled(!m_demo && !m_loadingArt && (m_page + 1) * 50 < m_total);
    m_favorite->setEnabled(selected);
    const bool favorite = selected && m_favorites.contains(m_game.key());
    m_favorite->setChecked(favorite);
    m_favorite->setToolTip(favorite ? "Quitar de favoritos de Art Studio" : "Marcar como favorito en Art Studio");
    m_favorite->setIcon(Ui::icon(Ui::Symbol::Star, QColor(favorite ? "#f5d58a" : "#a2b5c8")));
    m_typeChoice->setEnabled(selected); m_collectionTarget->setEnabled(selected);
    for (auto *b : findChildren<QPushButton *>()) {
        const auto id = b->property("testId").toString();
        if (id == "addToCollection") b->setEnabled(selected && !m_collectionTarget->currentData().toString().isEmpty());
        if (id == "removeQueued") b->setEnabled(!m_busy && m_queueList->currentRow() >= 0);
        if (id == "clearQueue") b->setEnabled(!m_busy && !m_pending.isEmpty());
    }
    if (auto *associate = findChild<QPushButton *>("associateTitle")) associate->setEnabled(selected && !m_busy && m_matchResults->currentData().toLongLong() > 0);
    const auto art = m_artModel.artwork(m_gallery->currentIndex().row());
    m_selectionInfo->setText(artwork ? m_gallery->currentIndex().data(SubtitleRole).toString() + " · " + art.author : "Selecciona un diseño para prepararlo");
    m_animated->setEnabled(m_type != "icon"); m_animated->setToolTip(m_type == "icon" ? "Steam utiliza iconos estáticos; ICO contiene resoluciones, no animación." : "Incluir diseños animados");
    m_connectButton->setText(m_network.hasApiKey() ? "SteamGridDB · Clave cargada" : "Conectar SteamGridDB");
    m_connectButton->setToolTip(m_network.hasApiKey() ? "Clave cargada; puedes cambiarla aquí. No implica que el servidor la haya validado." : "Consultar diseños de la comunidad con tu clave API");
    updateDownloads();
}
void Window::updateCurrentArt() {
    if (!m_inlineCurrent) return;
    m_currentFile.clear();
    if (m_game.appId) {
        if (!m_demo) m_currentFile = SteamScanner::existingArt(m_game, m_type);
        if (m_currentFile.isEmpty() && m_type == "capsule") m_currentFile = m_game.cover;
        if (m_currentFile.isEmpty() && m_type == "hero") m_currentFile = m_game.hero;
        if (m_currentFile.isEmpty() && m_type == "icon" && m_demo) m_currentFile = m_game.icon;
    }
    if (m_inlineCurrent && m_inlineCurrentFile != m_currentFile && !m_previewProposal.game.appId) {
        if (!m_currentFile.isEmpty()) m_cache.pin(m_currentFile);
        if (!m_inlineCurrentFile.isEmpty()) m_cache.unpin(m_inlineCurrentFile);
        m_inlineCurrentFile = m_currentFile; m_inlineCurrent->setFile(m_currentFile, false);
    }
    m_typeNotice->setVisible(m_type == "icon" && m_game.appId);
    m_typeNotice->setText(m_demo ? "Iconos de muestra · Estáticos, con transparencia. Aplicar a Steam está desactivado." : m_game.external ? "Acceso externo · Se respalda y actualiza solo su campo icon en shortcuts.vdf. Se ajusta a 256×256 sin deformar; cierra Steam antes de aplicar." : m_game.iconHash.isEmpty() ? "Steam no tiene un identificador local de icono para este juego. Puedes explorar, pero no aplicar; actualiza la biblioteca." : "Icono de Steam · Caché compartida entre cuentas; Steam puede regenerarla. Se ajusta a 256×256 sin deformar. Se guardará una copia; cierra Steam antes de aplicar.");
    if (!m_previewProposal.game.appId) m_currentInfo->setText(m_currentFile.isEmpty() ? "No hay imagen local para este formato." : artTitle(m_type) + " · " + (m_demo ? "Diseño de muestra" : "Lectura local, sin modificar"));
    m_gameInfo->setText(!m_game.appId ? "Elige un juego para empezar" : QString("%1 · %2").arg(m_game.external ? "Acceso externo" : m_game.installed ? "Instalado" : m_game.cachedOnly ? "Caché local, sin verificar licencia" : "Biblioteca local").arg(m_game.external ? "Steam" : "App " + QString::number(m_game.appId)));
    if (m_game.appId) m_gameInfo->setText(m_gameInfo->text() + " · " + artTitle(m_type));
    updateActions();
}
void Window::setCardDensity(int density) {
    const QList<QSize> sizes = {QSize(138, 240), QSize(164, 278), QSize(192, 320)};
    const QSize cell = sizes.at(qBound(0, density, 2));
    m_libraryDelegate->setCellSize(cell); m_libraryView->setGridSize(cell); m_libraryView->doItemsLayout();
    m_settings.setValue("view/density", density);
}
void Window::updateLayout() {
    if (!m_librarySection) return;
    m_librarySection->setFixedHeight(qBound(680, height() - 180, 800));
    if (m_activityLayout) m_activityLayout->setDirection(width() < 1280 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
}
void Window::resizeEvent(QResizeEvent *event) { QMainWindow::resizeEvent(event); updateLayout(); }
void Window::message(const QString &text, bool error) { m_status->setText(text); m_status->setStyleSheet(error ? "color:#efa99f" : "color:#a1b3c8"); m_statusIcon->setPixmap(Ui::icon(error ? Ui::Symbol::Warning : Ui::Symbol::Check, QColor(error ? "#efa99f" : "#71cbb4")).pixmap(18, 18)); m_statusIcon->setAccessibleName(error ? "Error" : "Estado"); }
void Window::scan(bool force) {
    if (m_demo) return;
    if (m_busy || m_scanner.isRunning()) { m_debounce.start(); return; }
    if (!force && SteamScanner::signature(m_watchedPaths) == m_signature) return;
    const auto root = m_rootChoice->currentData().toString(); if (root.isEmpty()) return;
    m_progress->show(); message("Leyendo Steam de forma local. Puedes seguir usando la interfaz…");
    if (m_games.isEmpty()) m_libraryView->setEmptyState("Leyendo tu biblioteca…", "La detección es local. No se ejecutan juegos ni se modifica Steam.");
    m_scanner.setFuture(QtConcurrent::run([root] { ScanSnapshot snapshot; snapshot.scan = SteamScanner::scan(root); snapshot.index.setGames(snapshot.scan.games); return snapshot; }));
}
void Window::checkChanges() { if (!m_demo && !m_busy && !m_scanner.isRunning() && SteamScanner::signature(m_watchedPaths) != m_signature) m_debounce.start(); }
void Window::refilter() {
    if (!m_libraryView) return;
    LibraryFilter filter; filter.text = m_search->text(); filter.status = m_filter->currentIndex(); filter.order = m_sort->currentIndex();
    filter.favoritesOnly = m_favoritesOnly->isChecked(); filter.favorites = m_favorites;
    filter.collectionOnly = !m_activeCollection.isEmpty();
    if (filter.collectionOnly) for (const auto &value : m_collections.value(m_activeCollection).toArray()) filter.collection.insert(value.toString());
    const QVector<Game> filtered = m_libraryIndex.query(filter);
    m_libraryModel.setGames(filtered, m_favorites); m_count->setText(QString("%1 / %2 juegos").arg(filtered.size()).arg(m_games.size()));
    m_libraryView->setEmptyState(m_games.isEmpty() ? "Tu biblioteca empieza aquí" : "No hay juegos en esta vista", m_games.isEmpty() ? "Abre Steam e inicia sesión, o selecciona su carpeta en Configuración." : "Cambia la búsqueda o los filtros. Los favoritos y las colecciones se guardan solo en Art Studio.");
    int current = -1; for (int i = 0; i < filtered.size(); ++i) if (filtered.at(i).key() == m_game.key()) { current = i; break; }
    if (current >= 0) { QSignalBlocker blocker(m_libraryView->selectionModel()); m_libraryView->setCurrentIndex(m_libraryModel.index(current)); }
    else if (!filtered.isEmpty()) m_libraryView->setCurrentIndex(m_libraryModel.index(0));
    else {
        clearInlinePreview();
        m_game = Game(); m_art.invalidate(); m_artModel.setArtworks({});
        m_gameTitle->setText("Selecciona un juego"); m_loadingArt = false; m_total = 0;
        m_artStatus->setText("No hay juegos con estos filtros."); m_pages->setText("Sin selección");
        m_gallery->setEmptyState("Selecciona un juego", "Aquí podrás explorar sus carátulas, fondos y logotipos.");
    }
    updateCurrentArt();
}
void Window::selectGame(const QModelIndex &index) {
    auto game = m_libraryModel.game(index.row()); if (!game.appId) return;
    const bool changed = game.key() != m_game.key(); if (changed) { clearInlinePreview(); m_matchResults->clear(); m_matchQuery->clear(); } m_game = game; m_gameTitle->setText(game.name); updateCurrentArt();
    const auto mapping = m_settings.value("sgdb/mappings").toMap().value(game.key()).toLongLong();
    if (mapping > 0) m_art.setMapping(game.key(), mapping);
    if (changed) { m_page = 0; fetchArt(); }
}
void Window::selectType(const QString &type) {
    if (!artTypes().contains(type)) return;
    clearInlinePreview();
    m_type = type; m_page = 0;
    const bool wide = type == "wide" || type == "hero"; m_artDelegate->setWide(wide); m_artDelegate->setContain(type == "logo" || type == "icon");
    const QSize cell = type == "icon" ? QSize(160, 232) : wide ? QSize(200, 174) : QSize(148, 250);
    m_artDelegate->setCellSize(cell); m_gallery->setGridSize(cell); updateCurrentArt(); fetchArt();
}
void Window::fetchArt() {
    if (!m_game.appId) return;
    auto keys = m_assetRequests.keys(); for (auto key : keys) if (m_assetRequests.value(key).purpose == "thumb") { m_assetRequests.remove(key); m_network.cancel(key); }
    m_loadingArt = false; m_total = 0; m_artModel.setArtworks({}); updateActions();
    if (m_demo) {
        if (!m_static->isChecked()) { m_gallery->setEmptyState("Sin diseños estáticos", "Los diseños ficticios de esta demostración son estáticos. Activa Estáticas para mostrarlos."); m_artStatus->setText("Demostración: solo diseños estáticos."); m_pages->setText("0 diseños"); return; }
        QVector<Artwork> artworks;
        const QSize dimensions = m_type == "icon" ? QSize(256, 256) : m_type == "wide" || m_type == "hero" ? QSize(920, 430) : QSize(480, 720);
        for (int i = 0; i < 9; ++i) { Artwork art; art.id = "demo-" + QString::number(i); art.type = m_type; art.author = QStringList{"Studio Original", "Nightwave", "Aurora Lab"}.at(i % 3); art.width = dimensions.width(); art.height = dimensions.height(); artworks.append(art); }
        m_artModel.setArtworks(artworks);
        for (int i = 0; i < artworks.size(); ++i) { const auto file = m_cache.root() + "/demo-art-" + QString::number(m_game.appId) + "-" + m_type + "-" + QString::number(i) + ".png"; if (!QFileInfo::exists(file)) (m_type == "icon" ? demoIcon(m_game.name, i + 5) : demoCover(m_game.name, i + 5, dimensions)).save(file); m_artModel.setThumbnail(i, file); }
        m_total = artworks.size(); m_pages->setText("9 diseños de muestra"); m_artStatus->setText("DISEÑOS DE MUESTRA · Selecciona uno para prepararlo"); updateActions(); return;
    }
    m_art.search(m_game, m_type, m_static->isChecked(), m_animated->isChecked(), m_adult->isChecked(), m_page);
}
void Window::requestAsset(const QString &purpose) {
    const auto art = m_artModel.artwork(m_gallery->currentIndex().row());
    if (!m_game.appId) { message("Selecciona un juego.", true); return; }
    if (purpose == "preview" && !m_gallery->currentIndex().isValid() && !m_currentFile.isEmpty()) { showPreview(m_currentFile, false, m_game.name + (m_demo ? " · muestra local" : " · actual en Steam")); return; }
    if (m_demo) { const auto index = m_gallery->currentIndex(); if (!index.isValid()) { message("Selecciona un diseño de muestra."); return; } const auto file = index.data(ImageRole).toString(); if (purpose == "preview") showPreview(file, false, m_game.name, {m_game, m_type, file, "Diseño de demostración", false}); else stage(m_game, m_type, file, "Diseño de demostración", false); return; }
    if (art.id.isEmpty()) {
        auto current = SteamScanner::existingArt(m_game, m_type);
        if (current.isEmpty() && m_type == "capsule") current = m_game.cover;
        if (current.isEmpty() && m_type == "hero") current = m_game.hero;
        if (purpose == "preview" && !current.isEmpty()) showPreview(current, false, m_game.name + " · imagen actual");
        else message("Selecciona primero una imagen de la galería o usa Imagen local.");
        return;
    }
    message(purpose == "preview" ? "Preparando vista previa…" : "Descargando el original para la cola…");
    if (m_transfers.enqueue(art.original, m_game.name + " · " + artTitle(art.type), {m_game, art, purpose})) m_downloadView->setCurrentIndex(m_transfers.index(m_transfers.rowCount() - 1));
}
void Window::assetReady(quint64 token, const QString &file, bool) {
    if (!m_assetRequests.contains(token)) return;
    const auto request = m_assetRequests.take(token);
    if (request.purpose == "thumb") { if (request.revision == m_artModel.revision()) m_artModel.setThumbnail(request.row, file); return; }
    if (request.purpose == "match") {
        if (request.game.key() != m_game.key()) return;
        QFile input(file); if (!input.open(QIODevice::ReadOnly) || input.size() > 2 * 1024 * 1024) return;
        const auto results = QJsonDocument::fromJson(input.readAll()).object().value("data").toArray();
        m_matchResults->clear();
        for (const auto &entry : results) {
            const auto object = entry.toObject(); const auto id = static_cast<qint64>(object.value("id").toDouble());
            if (id > 0) m_matchResults->addItem(object.value("name").toString() + " · SGDB " + QString::number(id), id);
        }
        message(m_matchResults->count() ? "Elige una coincidencia bajo la galería y pulsa Asociar título. Solo cambia Art Studio." : "SteamGridDB no ha encontrado resultados para ese título."); updateActions();
        return;
    }
}
void Window::originalReady(const ArtTask &task, const QString &file, bool cached) {
    if (task.purpose == "preview" && (task.game.key() != m_game.key() || task.art.type != m_type)) {
        message("Original descargado. Como cambiaste de juego o formato, no se ha sustituido la comparación. Para verlo, selecciona su juego/formato y repite la descarga desde Descargas."); return;
    }
    message(cached ? "Original reutilizado desde la caché." : "Original descargado. Steam todavía no se ha modificado.");
    if (task.purpose == "preview") showPreview(file, task.art.animated, task.game.name + " · " + artTitle(task.art.type), {task.game, task.art.type, file, task.art.author, task.art.animated});
    else stage(task.game, task.art.type, file, task.art.author, task.art.animated);
}
void Window::localImage() {
    if (!m_game.appId) { message("Selecciona un juego antes de preparar una imagen."); return; }
    const Game game = m_game; const QString type = m_type;
    const auto path = QFileDialog::getOpenFileName(this, type == "icon" ? "Elegir icono local" : "Elegir imagen local", {}, type == "icon" ? "Iconos e imágenes (*.ico *.png *.jpg *.jpeg *.webp *.gif)" : "Imágenes (*.png *.apng *.jpg *.jpeg *.webp *.gif)"); if (path.isEmpty()) return;
    QString error; bool animated = false; if (!Transactions::validImage(path, &animated, &error)) { message(error, true); return; }
    const auto cached = m_cache.importFile(path, &error); if (cached.isEmpty()) { message(error, true); return; }
    stage(game, type, cached, "Imagen local", animated);
}
bool Window::stage(const Game &game, const QString &type, const QString &file, const QString &description, bool animated) {
    if (!game.appId || !artTypes().contains(type)) { message("Selecciona un juego y un formato válidos.", true); return false; }
    if (m_busy) { message("Espera a que termine la operación actual."); return false; }
    QString error; bool detectedAnimation = false; if (!Transactions::validImage(file, &detectedAnimation, &error)) { message(error, true); return false; }
    animated = animated || detectedAnimation;
    if (type == "icon" && (animated || !Transactions::validIcon(file, &error))) { message(error.isEmpty() ? "Elige un icono estático." : error, true); return false; }
    if (type == "icon" && !m_demo && Transactions::destination(game, type).isEmpty()) { message("No se conoce un destino seguro para ese icono. Actualiza la biblioteca; Steam debe tener metadatos locales del juego.", true); return false; }
    const QString snapshot = m_cache.importFile(file, &error);
    if (snapshot.isEmpty()) { message(error, true); return false; }
    PendingChange next{game, type, snapshot, description, animated};
    QSet<QString> uniqueFiles; qint64 totalBytes = 0; int otherChanges = 0;
    for (const auto &existing : m_pending) {
        if (existing.key() == next.key()) continue;
        ++otherChanges;
        if (!uniqueFiles.contains(existing.file)) { uniqueFiles.insert(existing.file); totalBytes += QFileInfo(existing.file).size(); }
    }
    if (!uniqueFiles.contains(snapshot)) totalBytes += QFileInfo(snapshot).size();
    if (otherChanges >= 100 || totalBytes > 256LL * 1024 * 1024) { message("La cola admite hasta 100 cambios y 256 MiB de originales. Aplica o divide el lote antes de añadir más.", true); return false; }
    for (int i = 0; i < m_pending.size(); ++i) if (m_pending.at(i).key() == next.key()) { m_cache.unpin(m_pending.at(i).file); m_pending.removeAt(i); break; }
    m_cache.pin(snapshot); m_pending.append(next); const bool saved = saveDraft(); updateQueue();
    if (saved) message("Preparado: " + game.name + " · " + artTitle(type) + ". Revisa la cola antes de aplicar.");
    return saved;
}
void Window::updateQueue() {
    const auto selected = m_queueList->currentItem(); const QString selectedKey = selected ? selected->data(Qt::UserRole).toString() : QString(); const int oldRow = m_queueList->currentRow();
    m_queueList->clear(); int restoredRow = -1;
    for (const auto &change : m_pending) {
        auto item = new QListWidgetItem(Ui::icon(Ui::artSymbol(change.type)), change.game.name + "\n" + artTitle(change.type) + " · " + change.description + (change.animated ? " · animada" : ""), m_queueList); item->setData(Qt::UserRole, change.key());
        if (change.key() == selectedKey) restoredRow = m_queueList->row(item);
    }
    if (!m_pending.isEmpty()) m_queueList->setCurrentRow(restoredRow >= 0 ? restoredRow : qBound(0, oldRow, int(m_pending.size()) - 1));
    QSet<QString> games, files; qint64 bytes = 0;
    for (const auto &change : m_pending) { games.insert(change.game.key()); if (!files.contains(change.file)) { files.insert(change.file); bytes += QFileInfo(change.file).size(); } }
    m_queueSummary->setText(m_pending.isEmpty() ? "Sin cambios preparados" : QString("%1 %2 · %3 %4 · %5 de originales").arg(m_pending.size()).arg(m_pending.size() == 1 ? "imagen" : "imágenes").arg(games.size()).arg(games.size() == 1 ? "juego" : "juegos").arg(readableBytes(bytes)));
    m_queueList->setFixedHeight(qBound(70, int(m_pending.size()) * 68, 272));
    m_queueList->setVisible(!m_pending.isEmpty()); m_queueEmpty->setVisible(m_pending.isEmpty()); updateLayout();
    m_animationControls->setVisible(std::any_of(m_pending.cbegin(), m_pending.cend(), [](const PendingChange &change) { return change.animated && change.type != "icon"; }));
    m_apply->setText(m_pending.size() == 1 ? "Aplicar 1 cambio…" : QString("Aplicar %1 cambios…").arg(m_pending.size())); m_apply->setEnabled(!m_pending.isEmpty() && !m_busy && !m_demo);
    updateRestoreAction();
    updateActions();
}
bool Window::saveDraft() {
    QString error;
    if (!DraftStore(m_dataDirectory, m_cache.root(), m_demo).save(m_pending, &error)) {
        syncDraftBackupPins();
        message("No se ha podido guardar la cola. No cierres la aplicación hasta resolverlo: " + error, true);
        m_draftNotice->setText(error); m_draftNotice->show();
        return false;
    }
    syncDraftBackupPins();
    m_draftNotice->hide();
    return true;
}
void Window::syncDraftBackupPins() {
    const auto files = DraftStore(m_dataDirectory, m_cache.root(), m_demo).previousOriginals();
    const QSet<QString> next(files.begin(), files.end());
    for (const auto &file : m_draftBackupFiles - next) m_cache.unpin(file);
    for (const auto &file : next - m_draftBackupFiles) m_cache.pin(file);
    m_draftBackupFiles = next;
}
void Window::loadDraft() {
    const auto loaded = DraftStore(m_dataDirectory, m_cache.root(), m_demo).load(); m_pending = loaded.changes;
    for (const auto &change : m_pending) m_cache.pin(change.file);
    syncDraftBackupPins();
    if (!loaded.warning.isEmpty()) { m_draftNotice->setText(loaded.warning); m_draftNotice->show(); }
    updateQueue();
}
void Window::applyQueue() {
    if (m_demo || m_busy || m_pending.isEmpty()) return;
    const bool animations = std::any_of(m_pending.cbegin(), m_pending.cend(), [](const PendingChange &change) { return change.animated && change.type != "icon"; });
    const auto profile = animations ? static_cast<AnimationProfile>(m_animationProfile->currentData().toInt()) : AnimationProfile::Original;
    QStringList roots; bool icons = false, shared = false;
    for (const auto &change : m_pending) {
        const auto target = Transactions::destination(change.game, change.type);
        if (target.isEmpty()) { message("No se encuentra un destino seguro. Actualiza la biblioteca y prepara de nuevo el cambio.", true); return; }
        if (!roots.contains(target)) roots.append(target);
        if (change.type == "icon") {
            icons = true; shared |= !change.game.external;
            if (change.game.external) {
                const QString config = QFileInfo(QFileInfo(target).absolutePath()).absolutePath() + "/shortcuts.vdf";
                if (!roots.contains(config)) roots.append(config);
            }
        }
    }
    const QString iconNotice = !icons ? QString() : QString("\n\nICONOS: cierra Steam antes de continuar. Los accesos externos requieren actualizar su campo icon en shortcuts.vdf; se respalda el archivo completo.") + (shared ? " Los iconos nativos afectan a todas las cuentas de esta instalación y Steam puede regenerarlos." : "");
    QStringList shown = roots.mid(0, 8); if (shown.size() < roots.size()) shown.append(QString("… y %1 archivos más (ver detalles).").arg(roots.size() - shown.size()));
    const QString animationNotice = !animations ? QString() : "\n\nAnimaciones: " + animationProfileTitle(profile) + (profile == AnimationProfile::Original ? ". No se reduce resolución/FPS." : ". Se reducirán resolución/FPS de las copias aplicadas; los originales de la cola se conservan.");
    const auto prompt = QString("Se aplicarán %1 imágenes a la biblioteca local de Steam.\n\nSe guardarán copias restaurables antes de escribir. Es recomendable cerrar Steam para evitar cambios simultáneos.%2%3\n\nArchivos de destino:\n%4\n\n¿Aplicar este lote?").arg(m_pending.size()).arg(iconNotice, animationNotice, shown.join('\n'));
    if (!Ui::confirmSteamChange(this, "Confirmar cambios en Steam", prompt, icons, roots.size() > 8 ? roots.join('\n') : QString())) return;
    m_busy = true; m_apply->setEnabled(false); m_progress->show(); updateActions(); const auto changes = m_pending; const auto transactions = m_transactions;
    m_operation.setFuture(QtConcurrent::run([transactions, changes, profile] { return transactions.apply(changes, profile); }));
}
void Window::refreshBackups() {
    m_backups->clear(); for (const auto &value : m_transactions.history()) {
        const auto record = value.toObject(); const auto state = record.value("status").toString();
        auto item = new QListWidgetItem(record.value("created").toString() + "\n" + QString::number(record.value("changes").toInt()) + " imágenes · " + (state == "committed" ? "Disponible para restaurar" : state == "restored" ? "Restaurada" : state == "applying" || state == "restoring" || state == "needs-attention" ? "Operación incompleta: revisar" : "Cancelada"), m_backups);
        item->setData(Qt::UserRole, record.value("id").toString()); item->setData(Qt::UserRole + 1, state);
    }
    m_backups->setVisible(m_backups->count() > 0); m_backupEmpty->setVisible(m_backups->count() == 0);
    m_backups->setFixedHeight(qBound(80, m_backups->count() * 68, 272)); updateRestoreAction();
}
void Window::updateRestoreAction() {
    const auto item = m_backups ? m_backups->currentItem() : nullptr;
    if (m_restore) m_restore->setEnabled(!m_demo && !m_busy && m_pending.isEmpty() && item && QStringList{"committed", "applying", "restoring", "needs-attention"}.contains(item->data(Qt::UserRole + 1).toString()));
}
void Window::restoreBackup() {
    if (m_demo || m_busy) return;
    const auto item = m_backups->currentItem(); if (!item) return;
    if (!m_pending.isEmpty()) { message("Aplica o vacía la cola antes de restaurar una copia; se conserva el trabajo pendiente.", true); focusSection("queue"); return; }
    if (item->data(Qt::UserRole + 1).toString() == "restored") { message("Esta copia ya se restauró."); return; }
    QStringList targets; bool icons = false, shared = false;
    for (const auto &value : m_transactions.history()) {
        const auto manifest = value.toObject(); if (manifest.value("id").toString() != item->data(Qt::UserRole).toString()) continue;
        for (const auto &entry : manifest.value("files").toArray()) {
            const auto file = entry.toObject(); const auto path = Transactions::recordDestination(file);
            if (path.isEmpty()) { message("La copia contiene un destino no válido. No se ha restaurado ningún archivo.", true); return; }
            icons |= file.value("type").toString() == "icon" || file.value("scope").toString() == "shortcuts" || file.value("scope").toString() == "library-icon";
            shared |= file.value("scope").toString() == "library-icon";
            if (!targets.contains(path)) targets.append(path);
        }
    }
    QStringList shown = targets.mid(0, 8); if (shown.size() < targets.size()) shown.append(QString("… y %1 archivos más (ver detalles).").arg(targets.size() - shown.size()));
    const auto prompt = "¿Restaurar los archivos anteriores de este lote?\n\nSe comprobarán todos los archivos, incluido shortcuts.vdf si corresponde. Si otro programa los cambió, la operación se detendrá." + QString(icons ? "\n\nCierra Steam antes de restaurar iconos." : "") + (shared ? " La caché de iconos nativos se comparte entre cuentas." : "") + "\n\nDestinos:\n" + shown.join('\n');
    if (!Ui::confirmSteamChange(this, "Confirmar restauración", prompt, icons, targets.size() > 8 ? targets.join('\n') : QString())) return;
    m_busy = true; m_progress->show(); updateActions(); const auto transactions = m_transactions; const auto id = item->data(Qt::UserRole).toString();
    m_operation.setFuture(QtConcurrent::run([transactions, id] { return transactions.restore(id); }));
}
void Window::createCollection() {
    const auto name = m_collectionName->text().trimmed();
    if (name.isEmpty()) { message("Escribe el nombre de la colección en el campo visible sobre la biblioteca.", true); m_collectionName->setFocus(); return; }
    if (name.size() > 60 || name == "Todas las colecciones" || m_collections.contains(name)) { message("Ese nombre ya existe, está reservado o supera 60 caracteres.", true); return; }
    m_collections[name] = QJsonArray(); m_collectionChoice->addItem(name); m_collectionTarget->addItem(name, name); m_collectionTarget->setCurrentIndex(m_collectionTarget->count() - 1); m_collectionName->clear(); saveCollections(); message("Colección local creada: " + name); updateActions();
}
void Window::addToCollection() {
    if (!m_game.appId) return;
    const auto name = m_collectionTarget->currentData().toString();
    if (name.isEmpty() || !m_collections.contains(name)) { message("Crea o elige una colección antes de añadir el juego."); return; }
    auto entries = m_collections.value(name).toArray(); bool exists = false; for (const auto &entry : entries) if (entry.toString() == m_game.key()) exists = true;
    if (!exists) entries.append(m_game.key());
    m_collections[name] = entries; saveCollections(); message("Añadido a «" + name + "». Solo se ha actualizado esta aplicación."); refilter();
}
void Window::saveCollections() { m_settings.setValue("library/favorites", QStringList(m_favorites.begin(), m_favorites.end())); m_settings.setValue("library/collections", QJsonDocument(m_collections).toJson(QJsonDocument::Compact)); m_settings.sync(); }
void Window::manualSearch() {
    if (m_demo || !m_game.appId) return;
    if (!m_network.hasApiKey()) { connectService(); if (!m_network.hasApiKey()) return; }
    for (auto token : m_assetRequests.keys()) if (m_assetRequests.value(token).purpose == "match") { m_assetRequests.remove(token); m_network.cancel(token); }
    const auto title = m_matchQuery->text().trimmed();
    if (title.isEmpty() || title.size() > 200) { message("Escribe un título en el buscador de SteamGridDB bajo la galería."); m_matchQuery->setFocus(); return; }
    m_art.invalidate(); m_artStatus->setText("Buscando coincidencias. Tendrás que elegir el juego correcto…");
    const auto url = QUrl::fromEncoded(QByteArray("https://www.steamgriddb.com/api/v2/search/autocomplete/") + QUrl::toPercentEncoding(title));
    const auto token = m_network.fetch(url, "api"); m_assetRequests[token] = {"match", m_game, {}, -1, 0};
}
void Window::demoLibrary() {
    const QStringList names = {"Lunar Drift", "Neon Horizon", "Wildhaven", "Deep Signal", "Nova Frontier", "Emberfall", "Silent Orbit", "Coral Skies", "Echoes of Dawn", "Midnight Atlas", "Cloudbound", "Afterlight", "Velvet Ocean", "Solstice", "Iron Forest"};
    for (int i = 0; i < names.size(); ++i) { Game game; game.appId = i + 1; game.name = names.at(i); game.steamRoot = "demo"; game.userId = "0"; game.installed = i % 3 != 0; game.cover = m_cache.root() + "/demo-cover-" + QString::number(i) + ".png"; if (!QFileInfo::exists(game.cover)) demoCover(game.name, i).save(game.cover); game.icon = m_cache.root() + "/demo-icon-" + QString::number(i) + ".png"; if (!QFileInfo::exists(game.icon)) demoIcon(game.name, i).save(game.icon); m_games.append(game); }
    m_libraryIndex.setGames(m_games); m_favorites.insert(m_games.at(1).key()); refilter(); message("Demostración visual · no hay conexión a Steam ni cambios en su biblioteca.");
}
void Window::closeEvent(QCloseEvent *event) {
    if (m_busy) { message("Espera a que termine la aplicación o restauración antes de cerrar.", true); event->ignore(); return; }
    if (!saveDraft()) { event->ignore(); return; }
    QString prompt;
    if (!m_pending.isEmpty()) prompt += "La cola se ha guardado para continuar más tarde; no se aplicará al cerrar.\n\n";
    if (m_transfers.activeCount()) prompt += "Se cancelarán las descargas activas. Puedes volver a solicitarlas al abrir.\n\n";
    if (!prompt.isEmpty() && QMessageBox::question(this, "Cerrar Steam Art Studio", prompt + "¿Cerrar la aplicación?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { event->ignore(); return; }
    for (int row = 0; row < m_transfers.rowCount(); ++row) m_transfers.cancel(row);
    m_settings.setValue("window/geometry", saveGeometry()); m_settings.setValue("view/workspaceSplitter", m_split->saveState()); m_settings.sync(); QMainWindow::closeEvent(event);
}
void Window::changeEvent(QEvent *event) { QMainWindow::changeEvent(event); if (event->type() == QEvent::ActivationChange && isActiveWindow()) checkChanges(); }
void Window::saveScreenshot(const QString &path) { grab().save(path); }
}
