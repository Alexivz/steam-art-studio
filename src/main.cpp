#include "window.h"
#include "platform.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QIcon>
#include <QJsonDocument>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QSslSocket>
#include <cstdio>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {
bool privateDirectory(const QString &path, QString *error) {
    QDir directory(path); const QString marker = path + "/.steam-art-studio-data";
    if (QFileInfo::exists(marker)) {
        QFile owned(marker);
        if (!owned.open(QIODevice::ReadOnly) || owned.readAll() != "Steam Art Studio data v1\n") { *error = "La carpeta no tiene un identificador de datos válido."; return false; }
    }
    if (directory.exists() && !QFileInfo::exists(marker) && !directory.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty()) {
        *error = "La carpeta de datos no está vacía y no pertenece a Steam Art Studio. Se ha rechazado para proteger otros archivos."; return false;
    }
    if (!QDir().mkpath(path)) { *error = "No se ha podido crear la carpeta de datos."; return false; }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    if (!QFileInfo::exists(marker) && !Studio::CacheStore::atomicWrite(marker, "Steam Art Studio data v1\n", error)) return false;
    return true;
}
}
int main(int argc, char **argv) {
#ifdef Q_OS_UNIX
    ::umask(0077);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("SteamArtStudio"); QCoreApplication::setApplicationName("steam-art-studio"); QCoreApplication::setApplicationVersion("1.0.1");
    QApplication::setWindowIcon(QIcon(":/studio/steam-art-studio.svg"));
    QImageReader::setAllocationLimit(128);
    QCommandLineParser parser; parser.setApplicationDescription("Biblioteca y carátulas de Steam · aplicación independiente"); parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"demo", "Demostración visual sin modificar Steam."});
    parser.addOption({"smoke-test", "Abrir la demostración y salir automáticamente (requiere --demo)."});
    parser.addOption({"runtime-check", "Comprobar plugins y herramientas sin abrir datos ni detectar Steam."});
    parser.addOption({"screenshot", "Guardar captura de la demostración.", "archivo"});
    parser.addOption({"data-dir", "Carpeta de datos aislada, vacía o propiedad de la aplicación.", "carpeta"});
    parser.addOption({"cache-dir", "Carpeta de caché dedicada para pruebas.", "carpeta"});
    parser.addOption({"scan-only", "Mostrar un resumen de detección local, sin ventana ni cambios en Steam."});
    parser.addOption({"scan-root", "Carpeta de Steam para la detección de solo lectura.", "carpeta"});
    parser.process(app);
    if (parser.isSet("runtime-check")) {
        const auto supported = QImageReader::supportedImageFormats();
        QJsonArray formats; for (const auto &format : supported) formats.append(QString::fromLatin1(format));
        const bool tls = QSslSocket::supportsSsl();
        const QJsonObject report{{"version", QCoreApplication::applicationVersion()}, {"qt", qVersion()}, {"tls", tls}, {"image_formats", formats}, {"ffmpeg_available", !Studio::Platform::findTool("ffmpeg").isEmpty()}, {"steam_writes", false}};
        std::fputs(QJsonDocument(report).toJson(QJsonDocument::Compact).constData(), stdout); std::fputc('\n', stdout);
        return tls && supported.contains("png") && supported.contains("webp") && supported.contains("ico") ? 0 : 1;
    }
    if (parser.isSet("scan-only")) {
        QString root = parser.value("scan-root");
        if (root.isEmpty()) { const auto roots = Studio::SteamScanner::discover(); if (!roots.isEmpty()) root = roots.first().path; }
        const auto scan = Studio::SteamScanner::scan(root); int installed = 0, external = 0;
        for (const auto &game : scan.games) { installed += game.installed; external += game.external; }
        const QJsonObject data{{"games", scan.games.size()}, {"installed", installed}, {"external", external}, {"steam_writes", false}, {"error", scan.error}, {"warnings", QJsonArray::fromStringList(scan.warnings)}};
        std::fputs(QJsonDocument(data).toJson(QJsonDocument::Compact).constData(), stdout); std::fputc('\n', stdout); return scan.error.isEmpty() ? 0 : 1;
    }
    const bool demo = parser.isSet("demo");
    if (parser.isSet("smoke-test") && !demo) { std::fputs("--smoke-test requiere --demo para no cerrar una sesión real.\n", stderr); return 2; }
    QString data = parser.value("data-dir"); if (data.isEmpty()) data = Studio::Platform::dataDirectory(demo);
    QString cache = parser.value("cache-dir"); if (cache.isEmpty()) cache = Studio::Platform::cacheDirectory(demo);
    QString error;
    if (!Studio::DraftStore::separateDirectories(data, cache)) { std::fputs("Los datos y la caché deben estar en carpetas separadas, sin contenerse entre sí.\n", stderr); return 2; }
    if (!privateDirectory(data, &error) || !privateDirectory(cache, &error)) { QMessageBox::critical(nullptr, "No se pudo iniciar", error); return 1; }
    QLockFile lock(data + "/application.lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock()) { QMessageBox::information(nullptr, "Steam Art Studio", "Ya hay una ventana de esta aplicación usando los mismos datos. No se abrirá otra instancia sobre ella."); return 0; }
    Studio::Window window(data, cache, demo); window.show();
    if (demo && (!parser.value("screenshot").isEmpty() || parser.isSet("smoke-test"))) {
        const auto screenshot = parser.value("screenshot");
        QTimer::singleShot(3500, &window, [&window, screenshot, &app, &parser] { if (!screenshot.isEmpty()) window.saveScreenshot(screenshot); if (parser.isSet("smoke-test")) app.quit(); });
    }
    return app.exec();
}
