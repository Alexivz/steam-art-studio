# Licencias y dependencias

El código propio de Steam Art Studio se distribuye bajo **GPL-3.0-only** por
elección del usuario. El texto completo está en `LICENSE`. Los metadatos AppStream
de `packaging/linux/` están disponibles bajo CC0-1.0. El icono de la aplicación
es un recurso propio; no se incluyen imágenes de juegos en las distribuciones.

## Qt

La aplicación enlaza dinámicamente con Qt 6 Core/Gui/Widgets/Network/Concurrent/Svg
y, solo en Linux cuando está disponible, DBus/KWallet. Qt y sus plugins no son
código propio. Según el módulo/edición, se ofrecen bajo GPL-3.0 y/o LGPL-3.0 y otras
licencias para componentes incluidos. No se usa una licencia comercial de Qt.

Antes de publicar un paquete con DLL/bibliotecas de Qt hay que incluir los avisos
y textos de licencia de **esa versión y compilación** y dar acceso al código fuente
correspondiente de las dependencias distribuidas, con sus parches/build recipes.
Los scripts de despliegue no cambian la licencia de Qt ni omiten esas obligaciones.
Fuentes oficiales: <https://download.qt.io/official_releases/qt/>.

Los paquetes DEB/RPM dinámicos no incluyen Qt: lo resuelve el gestor del sistema.
AppImage, Flatpak y Windows pueden incluir bibliotecas adicionales; sus avisos
deben acompañar al artefacto y la publicación debe conservar las fuentes/recetas.

## FFmpeg opcional

FFmpeg se ejecuta como proceso local, no se enlaza con la aplicación. Una copia
instalada por el usuario conserva su propia licencia. Si se incluye en un paquete,
se debe acompañar de su licencia, versión/configuración y código fuente correspondiente.
Las recetas limitadas de este proyecto no habilitan `--enable-gpl` ni `--enable-nonfree`.
Fuentes: <https://ffmpeg.org/releases/>. Un FFmpeg antiguo puede no decodificar WebP
animado; la aplicación debe informar del error y conservar la cola.

## Herramientas y publicación

CMake/CPack, NSIS, linuxdeploy y los SDK de Flatpak son herramientas de construcción;
no pasan a ser código propio. Sus licencias y las de los runtimes se mantienen.
Los hashes identifican archivos y detectan cambios; no sustituyen una firma.

Canal del proyecto: <https://github.com/Alexivz/steam-art-studio>. No se afirma
haber firmado/verificado por Microsoft, Valve, SteamGridDB o Flathub. Se conserva
el identificador de aplicación anterior por compatibilidad de datos; el canal
de esta entrega es GitHub, no Flathub. Conservar licencias/avisos/fuentes de cada
artefacto al redistribuirlo. `LICENSE` no licencia imágenes de
SteamGridDB, marcas o contenido propiedad de terceros.
