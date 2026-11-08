#include "window.h"
#include "credentials.h"
#include "diagnostics.h"
#include "imageutils.h"
#include "preview.h"
#include "platform.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QImageReader>
#include <QJsonDocument>
#include <QMessageBox>
#include <QScrollBar>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace Studio {
namespace {
QLabel *text(const QString &value, const QString &name = "muted") {
    auto label = new QLabel(value); label->setObjectName(name); label->setTextFormat(Qt::PlainText); label->setWordWrap(true); return label;
}
QPushButton *action(const QString &value, Ui::Symbol symbol) {
    auto button = new QPushButton(value); button->setIcon(Ui::icon(symbol)); button->setCursor(Qt::PointingHandCursor); return button;
}
}
void Window::focusSection(const QString &name) {
    auto section = m_sections.value(name);
    if (!section || !m_workspaceScroll) return;
    m_workspaceScroll->widget()->layout()->activate();
    const int top = section->mapTo(m_workspaceScroll->widget(), QPoint()).y();
    m_workspaceScroll->verticalScrollBar()->setValue(top);
}
void Window::buildInlinePreview(QVBoxLayout *layout) {
    auto section = new QWidget; section->setObjectName("previewSection"); m_sections["preview"] = section;
    auto box = new QVBoxLayout(section); box->setContentsMargins(0, 0, 0, 0); box->setSpacing(10);
    box->addWidget(text("Comparación y animación", "sectionTitle"));
    m_inlineTitle = text("Compara aquí sin abrir otras ventanas"); m_inlineTitle->setObjectName("inlinePreviewTitle"); box->addWidget(m_inlineTitle);
    auto playbackNote = text("Animaciones a su ritmo original, sin límite fijo de 20 FPS. La imagen se ajusta al espacio visible; el archivo no cambia. La fluidez depende del original y del equipo."); playbackNote->setWordWrap(true); box->addWidget(playbackNote);
    auto tools = new QHBoxLayout; m_inlineInfo = text("La imagen actual es de solo lectura. Selecciona un diseño y pulsa Vista previa."); tools->addWidget(m_inlineInfo, 1);
    tools->addWidget(text("Fondo")); auto background = new QComboBox; background->setObjectName("previewBackground"); background->setAccessibleName("Fondo de la comparación"); background->addItems({"Cuadrícula", "Oscuro", "Claro"}); tools->addWidget(background); box->addLayout(tools);
    auto comparison = new QHBoxLayout; comparison->setSpacing(16);
    auto before = new QWidget; before->setObjectName("comparisonBefore"); auto beforeLayout = new QVBoxLayout(before); beforeLayout->setContentsMargins(0, 0, 0, 0);
    beforeLayout->addWidget(text(m_demo ? "MUESTRA ACTUAL" : "ACTUAL EN STEAM", "eyebrow")); m_currentInfo = text("Sin imagen disponible"); beforeLayout->addWidget(m_currentInfo); m_inlineCurrent = new Preview; m_inlineCurrent->setObjectName("currentPreview"); m_inlineCurrent->setFixedHeight(300); beforeLayout->addWidget(m_inlineCurrent); comparison->addWidget(before, 1);
    auto after = new QWidget; after->setObjectName("comparisonAfter"); auto afterLayout = new QVBoxLayout(after); afterLayout->setContentsMargins(0, 0, 0, 0);
    afterLayout->addWidget(text("DISEÑO NUEVO · SIN APLICAR", "eyebrow")); m_inlineNew = new Preview; m_inlineNew->setObjectName("newPreview"); m_inlineNew->setFixedHeight(300); afterLayout->addWidget(m_inlineNew); comparison->addWidget(after, 1); box->addLayout(comparison);
    auto controls = new QHBoxLayout; m_inlinePlay = action("Reproducir animación", Ui::Symbol::Play); m_inlinePlay->setObjectName("playInlineAnimation"); m_inlinePlay->setEnabled(false); controls->addWidget(m_inlinePlay);
    auto clear = action("Limpiar comparación", Ui::Symbol::Close); clear->setObjectName("clearPreview"); controls->addWidget(clear); controls->addStretch();
    m_inlinePrepare = action("Preparar este diseño", Ui::Symbol::Plus); m_inlinePrepare->setProperty("intent", "primary"); m_inlinePrepare->setObjectName("prepareFromPreview"); m_inlinePrepare->setEnabled(false); controls->addWidget(m_inlinePrepare); box->addLayout(controls); layout->addWidget(section);
    connect(background, &QComboBox::currentIndexChanged, m_inlineCurrent, &Preview::setBackgroundMode); connect(background, &QComboBox::currentIndexChanged, m_inlineNew, &Preview::setBackgroundMode);
    connect(m_inlinePlay, &QPushButton::clicked, m_inlineNew, &Preview::togglePlayback);
    connect(m_inlineNew, &Preview::playbackChanged, this, [this](bool playing) { m_inlinePlay->setText(playing ? "Pausar animación" : "Reproducir animación"); m_inlinePlay->setIcon(Ui::icon(playing ? Ui::Symbol::Pause : Ui::Symbol::Play)); });
    connect(m_inlineNew, &Preview::error, this, [this](const QString &error) { message(error, true); });
    connect(clear, &QPushButton::clicked, this, [this] { clearInlinePreview(); updateCurrentArt(); });
    connect(m_inlinePrepare, &QPushButton::clicked, this, [this] {
        const auto proposal = m_previewProposal;
        if (proposal.game.appId && stage(proposal.game, proposal.type, proposal.file, proposal.description, proposal.animated)) focusSection("queue");
    });
    connect(m_workspaceScroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        QTimer::singleShot(0, this, [this] { if (m_inlineNew->playing() && m_inlineNew->visibleRegion().isEmpty()) m_inlineNew->stop(); });
    });
}
void Window::clearInlinePreview() {
    if (!m_inlineNew) return;
    m_inlineNew->setFile({}, false); m_inlineCurrent->setFile({}, false);
    if (!m_inlineFile.isEmpty()) m_cache.unpin(m_inlineFile);
    if (!m_inlineCurrentFile.isEmpty()) m_cache.unpin(m_inlineCurrentFile);
    m_inlineFile.clear(); m_inlineCurrentFile.clear(); m_previewProposal = {};
    m_inlineTitle->setText("Compara aquí sin abrir otras ventanas"); m_inlineInfo->setText("Selecciona un diseño y pulsa Vista previa. Preparar no aplica cambios a Steam.");
    m_inlinePlay->setEnabled(false); m_inlinePrepare->setEnabled(false);
}
void Window::showPreview(const QString &file, bool animated, const QString &title, const PendingChange &proposal) {
    if (!QFileInfo(file).isFile()) { message("El original de la vista previa ya no está disponible.", true); return; }
    if (!animated) Transactions::validImage(file, &animated);
    QString current = proposal.game.appId ? QString() : m_currentFile;
    if (proposal.game.appId) {
        if (!m_demo) current = SteamScanner::existingArt(proposal.game, proposal.type);
        if (current.isEmpty() && proposal.type == "capsule") current = proposal.game.cover;
        if (current.isEmpty() && proposal.type == "hero") current = proposal.game.hero;
        if (current.isEmpty() && proposal.type == "icon" && m_demo) current = proposal.game.icon;
    }
    // Pin before releasing the previous preview, including when paths coincide.
    m_cache.pin(file); if (!current.isEmpty()) m_cache.pin(current);
    clearInlinePreview(); m_inlineFile = file; m_inlineCurrentFile = current;
    m_previewProposal = proposal; m_previewProposal.animated = animated;
    m_inlineCurrent->setFile(current, false); m_inlineNew->setFile(file, animated);
    m_currentInfo->setText(current.isEmpty() ? "No hay imagen actual para comparar." : proposal.game.appId ? artTitle(proposal.type) + " · " + proposal.game.name : "Lectura local, sin modificar");
    m_inlineTitle->setText(title); QImageReader reader(file); selectLargestIcon(reader); const auto size = reader.size();
    m_inlineInfo->setText(QString("%1 × %2 píxeles · %3 · %4 MiB · sin aplicar").arg(size.width()).arg(size.height()).arg(animated ? "Animación" : "Imagen estática").arg(QFileInfo(file).size() / (1024. * 1024.), 0, 'f', 1));
    m_inlinePlay->setEnabled(animated); m_inlinePrepare->setEnabled(proposal.game.appId && !m_busy); focusSection("preview");
}
void Window::buildInlineSettings(QVBoxLayout *layout) {
    auto section = new QWidget; section->setObjectName("settingsSection"); m_sections["settings"] = section; auto box = new QVBoxLayout(section); box->setContentsMargins(0, 0, 0, 0); box->setSpacing(10);
    box->addWidget(text("Conexión, ajustes y privacidad", "sectionTitle"));
    box->addWidget(text("La biblioteca local funciona sin clave. Las consultas nuevas requieren SteamGridDB. La clave solo permanece en memoria o, si lo eliges, en KWallet; nunca en JSON ni en diagnósticos."));
    auto credentials = new QHBoxLayout; m_keyInput = new QLineEdit; m_keyInput->setObjectName("apiKeyInput"); m_keyInput->setEchoMode(QLineEdit::Password); m_keyInput->setPlaceholderText("Clave API de SteamGridDB · solo se muestra enmascarada"); m_keyInput->setEnabled(!m_demo); credentials->addWidget(m_keyInput, 1);
    auto connect = action("Cargar clave", Ui::Symbol::Link); connect->setObjectName("saveConnection"); connect->setEnabled(!m_demo); credentials->addWidget(connect);
    auto disconnect = action("Olvidar en memoria", Ui::Symbol::Close); disconnect->setObjectName("disconnectService"); disconnect->setEnabled(!m_demo); credentials->addWidget(disconnect); box->addLayout(credentials);
    m_rememberKey = new QCheckBox("Recordar de forma segura en KWallet (opcional)"); m_rememberKey->setEnabled(!m_demo && Credentials::available());
#ifdef Q_OS_WIN
    m_rememberKey->setText("En Windows, la clave solo permanece en memoria durante esta sesión");
#endif
    box->addWidget(m_rememberKey);
    QObject::connect(connect, &QPushButton::clicked, this, &Window::saveConnection); QObject::connect(m_keyInput, &QLineEdit::returnPressed, this, &Window::saveConnection);
    QObject::connect(disconnect, &QPushButton::clicked, this, [this] { m_art.invalidate(); m_network.setApiKey({}); m_keyInput->clear(); m_settings.setValue("wallet/enabled", false); m_settings.sync(); fetchArt(); updateActions(); message("Clave retirada de memoria y carga automática desactivada. No se ha borrado la entrada de KWallet."); });
    auto links = new QHBoxLayout; auto website = action("Obtener clave gratuita", Ui::Symbol::Link); auto import = action("Importar clave anterior…", Ui::Symbol::Backup); import->setEnabled(!m_demo); links->addWidget(website); links->addWidget(import); links->addStretch(); box->addLayout(links);
#ifdef Q_OS_WIN
    import->setEnabled(false); import->setToolTip("La importación del gestor anterior solo está implementada para su configuración Linux. Puedes introducir tu clave manualmente.");
#endif
    QObject::connect(website, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://www.steamgriddb.com/profile/preferences/api")); });
    QObject::connect(import, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Importar únicamente la clave", "¿Leer únicamente la clave de SteamGridDB de Steam Art Manager? No se modificarán sus ajustes. La clave no se guardará salvo elección explícita de KWallet.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        QFile file(QDir::homePath() + "/.config/dev.tormak.steam-art-manager/settings.json");
        if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) { message("No se ha encontrado una configuración anterior legible.", true); return; }
        const auto key = QJsonDocument::fromJson(file.readAll()).object().value("steamGridDbApiKey").toString();
        if (Credentials::valid(key)) m_keyInput->setText(key); else message("No se ha encontrado una clave válida para importar.", true);
    });
    box->addWidget(m_manualControls);
    auto maintenance = new QHBoxLayout; auto clear = action("Limpiar caché…", Ui::Symbol::Trash); clear->setObjectName("clearCache"); auto pick = action("Elegir carpeta de Steam…", Ui::Symbol::Collection); pick->setEnabled(!m_demo); auto data = action("Abrir datos y copias", Ui::Symbol::Backup); maintenance->addWidget(clear); maintenance->addWidget(pick); maintenance->addWidget(data); maintenance->addStretch(); box->addLayout(maintenance);
    QObject::connect(clear, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Limpiar caché", "¿Borrar descargas no utilizadas? Se conservarán originales fijados, cola, copia anterior del borrador y copias de seguridad.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        m_cache.clearUnpinned(); message("Caché no utilizada eliminada; cola, comparación y copias conservadas.");
    });
    QObject::connect(pick, &QPushButton::clicked, this, [this] { const auto folder = QFileDialog::getExistingDirectory(this, "Carpeta de Steam"); if (folder.isEmpty()) return; if (!QFileInfo(folder + "/appcache/appinfo.vdf").isFile()) { message("La carpeta no contiene appcache/appinfo.vdf.", true); return; } m_rootChoice->addItem("Steam · carpeta seleccionada", folder); m_rootChoice->setCurrentIndex(m_rootChoice->count() - 1); });
    QObject::connect(data, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataDirectory)); });
    m_cacheLabel = text("Caché persistente · 512 MiB. Los originales fijados pueden superar temporalmente el presupuesto."); box->addWidget(m_cacheLabel);
    box->addWidget(text("Diagnóstico local · revisa el contenido antes de guardar. No incluye claves, cuentas, rutas personales, URLs ni nombres de juegos. No se envía a ningún servidor."));
    auto diagnosticActions = new QHBoxLayout; auto generate = action("Preparar diagnóstico", Ui::Symbol::Refresh); generate->setObjectName("exportDiagnostics"); auto save = action("Guardar diagnóstico…", Ui::Symbol::Save); save->setObjectName("saveDiagnostics"); save->setEnabled(false); diagnosticActions->addWidget(generate); diagnosticActions->addWidget(save); diagnosticActions->addStretch(); box->addLayout(diagnosticActions);
    m_diagnosticContents = new QTextEdit; m_diagnosticContents->setObjectName("diagnosticPreview"); m_diagnosticContents->setReadOnly(true); m_diagnosticContents->setFixedHeight(180); m_diagnosticContents->setPlaceholderText("Pulsa Preparar diagnóstico para revisar aquí el JSON completo, sin abrir ventanas."); box->addWidget(m_diagnosticContents);
    QObject::connect(generate, &QPushButton::clicked, this, [this, save] {
        m_diagnosticContents->setPlainText(QString::fromUtf8(diagnosticReport(m_games.size(), m_cache.bytes(), m_pending.size(), m_transfers.activeCount(), !Platform::findTool("ffmpeg").isEmpty(), m_demo))); save->setEnabled(true);
    });
    QObject::connect(save, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(this, "Guardar diagnóstico local", "art-studio-diagnostico.json", "JSON (*.json)"); if (path.isEmpty()) return;
        QString error; if (!CacheStore::atomicWrite(path, m_diagnosticContents->toPlainText().toUtf8(), &error)) { message("No se pudo guardar el diagnóstico: " + error, true); return; }
        message("Diagnóstico local guardado; no se ha enviado a ningún servidor.");
    });
    box->addWidget(text("Ctrl+F buscar · Ctrl+1 biblioteca · Ctrl+2 favoritos · Ctrl+3 colección · Ctrl+4 cola · Ctrl+5 copias · Ctrl+6 descargas · Ctrl+Intro preparar · F5 actualizar. Los atajos desplazan esta misma pantalla; ninguno aplica cambios."));
    box->addWidget(text("Steam Art Studio · " + QCoreApplication::applicationVersion() + " · versión pública del proyecto. Aplicación independiente, no oficial de Valve. FFmpeg: " + (Platform::findTool("ffmpeg").isEmpty() ? "no disponible" : "disponible")));
    layout->addWidget(section);
}
void Window::connectService() {
    focusSection("settings");
    if (m_demo) { message("La demostración no conecta cuentas ni carga claves."); return; }
    m_keyInput->setFocus(); message("Introduce tu clave en el campo de Conexión y pulsa Cargar clave.");
}
void Window::saveConnection() {
    if (m_demo) return;
    const auto value = m_keyInput->text().trimmed();
    if (!Credentials::valid(value)) { message("La clave no tiene un formato válido. No incluyas espacios ni «Bearer».", true); return; }
    m_art.invalidate(); m_network.setApiKey(value); QString walletError;
    if (m_rememberKey->isChecked()) { const bool saved = Credentials::store(value, &walletError); m_settings.setValue("wallet/enabled", saved); }
    else m_settings.setValue("wallet/enabled", false);
    m_keyInput->clear(); m_settings.sync(); fetchArt(); updateActions();
    message(walletError.isEmpty() ? "Clave cargada, pendiente de validación por SteamGridDB. No se ha guardado en JSON ni en registros." : "Clave solo en memoria: " + walletError, !walletError.isEmpty());
}
}
