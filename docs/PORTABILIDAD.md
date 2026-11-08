# Distribución multiplataforma — 1.0.0

Primera versión oficial del proyecto independiente, no certificación universal ni
afiliación con Valve. Canal: <https://github.com/Alexivz/steam-art-studio/releases>.
La página única y las funciones de biblioteca, comparación, cola, animaciones,
descargas y copias son las mismas. No hay autoactualización ni firma de código.
Abrir el EXE/AppImage directamente, **sin --demo**. Ver
[calidad original y modo normal](CALIDAD-Y-MODO-NORMAL.md).

## Elegir formato

| Formato | Requisitos/uso |
| --- | --- |
| AppImage | Linux x86_64 con glibc 2.35+ (base Ubuntu 22.04), Qt/FFmpeg incluidos. |
| Flatpak | Linux x86_64, Flatpak y `org.freedesktop.Platform//26.08`. No es Flathub. |
| DEB | Ubuntu 22.04+/Debian compatible con Qt 6.2+; dependencias del gestor. |
| RPM | Candidato dinámico para distribuciones RPM con Qt 6.2+; probar con su gestor. |
| ZIP Windows | Extraer completo y abrir `steam-art-studio.exe`; no mover solo el EXE. |
| Instalador Windows | Instalación por usuario en `%LOCALAPPDATA%/Programs/Steam Art Studio`, sin administrador. |
| Fuentes | Código, recursos, pruebas, recetas y GPL; dependencias se suministran aparte. |

No cubre Linux musl/Alpine, glibc anterior, ARM, Windows ARM ni macOS. Objetivo
Windows: Windows 10/11 x64 actualizados; la validación automatizada aquí es bajo
Wine, no una certificación Windows nativa. Un paquete RPM construido
en Ubuntu no equivale a una prueba de instalación en todas las distribuciones RPM.
AppImage presupone un sistema de escritorio con EGL/OpenGL, fuentes y certificados
de confianza del sistema; no incluye controladores gráficos ni convierte una imagen
Ubuntu mínima sin esas bibliotecas en un escritorio completo.

## Instalar

Los archivos y hashes están en GitHub Releases y, al construir, en `dist/1.0.0/`. Comprueba los hashes
antes de ejecutar. No prueban la identidad del autor; no hay firma de código.

```bash
# AppImage: sin instalar. Si no hay FUSE, usa --appimage-extract-and-run.
chmod +x steam-art-studio-1.0.0-linux-x86_64.AppImage
./steam-art-studio-1.0.0-linux-x86_64.AppImage

# Flatpak local: añadir Flathub si todavía no está configurado para este usuario.
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.freedesktop.Platform//26.08
flatpak install --user ./steam-art-studio-1.0.0-linux-x86_64.flatpak
flatpak run org.steamartstudio.SteamArtStudio

# Paquetes nativos: revisa antes los cambios propuestos por el gestor.
sudo apt install ./steam-art-studio-1.0.0-Linux-x86_64.deb
sudo dnf install ./steam-art-studio-1.0.0-Linux-x86_64.rpm
```

Flatpak concede red, gráficos y **solo raíces Steam concretas**: `~/.local/share/Steam`,
`~/.steam` y los datos Steam Flatpak. No concede todo home/host ni ejecución de
comandos del host. Una instalación Steam fuera de esas rutas puede necesitar un
permiso explícito del usuario; no se amplían permisos automáticamente. Las imágenes
locales se eligen con el selector del sistema/portal. El ID anterior se conserva
por compatibilidad; esta entrega no es una publicación en Flathub.

Windows: el instalador no arranca ni cierra aplicaciones. Exige carpeta vacía y
rechaza enlaces/junctions. Para actualizar, cierra tú Art Studio, desinstala el
programa anterior y ejecuta el nuevo instalador. **No borra datos, cola, caché ni
copias Qt.** El desinstalador solo enumera archivos del programa, sin borrado
recursivo; archivos desconocidos pueden dejar carpetas no vacías. ZIP y versión
instalada comparten los datos del mismo usuario; no abrirlas a la vez.

## Construir

```bash
cmake -S . -B build-cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --parallel 3
ctest --test-dir build-cmake --output-on-failure
```

Linux: usar `packaging/linux/Containerfile` sobre Ubuntu 22.04 para no elevar la
glibc mínima accidentalmente. Windows cruzado usa Fedora/MinGW y el toolchain
explícito `packaging/windows/mingw-toolchain.cmake`; no el wrapper Qt defectuoso.

```bash
python3 packaging/tools/build_ffmpeg.py --cache /ruta/inputs --output /ruta/ffmpeg
# MinGW/Fedora: añade --windows-cross. Prefijos Linux/Windows siempre separados.
cmake -S . -B build-cmake -DSTUDIO_BUNDLED_FFMPEG_DIR=/ruta/ffmpeg
PATH="/ruta/ffmpeg/bin:$PATH" ctest --test-dir build-cmake --output-on-failure
cpack --config build-cmake/CPackConfig.cmake -G DEB -B /ruta/paquetes
cpack --config build-cmake/CPackConfig.cmake -G RPM -B /ruta/paquetes

python3 packaging/tools/fetch_tool.py linuxdeploy --directory /ruta/tools
python3 packaging/tools/fetch_tool.py linuxdeploy-plugin-qt --directory /ruta/tools
python3 packaging/tools/fetch_tool.py runtime-x86_64 --directory /ruta/tools
python3 packaging/tools/appimage_sources.py --output /ruta/runtime-source --notices /ruta/runtime-notices
python3 packaging/linux/appimage.py --build build-cmake --appdir /ruta/AppDir --tools /ruta/tools --runtime-notices /ruta/runtime-notices --output /ruta/nueva.AppImage
python3 packaging/flatpak/bundle.py --appdir /ruta/AppDir --build /ruta/flatpak-build --repo /ruta/repo --output /ruta/nuevo.flatpak
```

MinGW: instalar primero a un stage dedicado con `cmake --install`; ejecutar
`packaging/windows/deploy_cross.py --stage /ruta/stage` para DLL/plugins/avisos.
En Windows nativo, CMake usa windeployqt al instalar. Después:

```bash
python3 packaging/windows/package.py --stage /ruta/stage --output /ruta/paquetes
makensis /ruta/paquetes/steam-art-studio-1.0.0-windows-x86_64.nsi
python3 packaging/tools/archive_source.py --output /ruta/fuentes.tar.gz
```

Los empaquetadores rechazan salidas ya existentes. El ZIP portable no instala
registro ni guarda datos junto al EXE. El instalador NSIS usa HKCU, no HKLM.
El workflow manual `.github/workflows/portable.yml` está preparado pero no ha
sido ejecutado en GitHub; su candidato Windows nativo no incluye FFmpeg todavía.

## Dependencias y redistribución

No redistribuir el FFmpeg completo del equipo de desarrollo: contiene componentes
`nonfree`. Los paquetes usan una compilación **limitada de FFmpeg 9.0.2** sin
GPL/nonfree/red, con WebP/GIF/APNG, su configuración, licencia y archivo fuente
oficial verificado. Las bibliotecas externas conservan sus propias licencias.

El código propio es GPL-3.0-only. Los binarios Qt, OpenSSL, WebP y demás bibliotecas
conservan sus avisos. Los inventarios exactos son `ubuntu-packages.json` y
`fedora-packages.json` en la documentación de cada bundle. Las fuentes de esas
dependencias se recogen con `packaging/tools/dependency_sources.py`; en Ubuntu
requiere índices deb-src y en Fedora verifica las firmas de los SRPM.

Antes de una publicación pública, conservar junto a los binarios el código propio,
fuentes/recetas de dependencias, licencias y hashes; revisar también las obligaciones
del runtime Flatpak y runtime AppImage. Ver `THIRD_PARTY_NOTICES.md`.

## Matriz de validación

Se distingue ejecución Linux local/contenedor, Windows emulado con Wine e
informes de probadores. La misma base funcional pasó 15 pruebas de portabilidad,
6 de reproducción, 70 de núcleo, 24 de red, 24 de GUI y 14 de empaquetado/
distribución, sin fallos ni omisiones. Los totales QtTest incluyen inicio/cierre.
Los resultados de los paquetes 1.0.0 acompañan la release en `VALIDACION.md`.
No se ha usado Steam real ni credenciales de SteamGridDB durante estas pruebas,
ni instalado paquetes en el perfil real del desarrollador. La prueba inicial de
un usuario Windows confirma interfaz/aplicación estática; no sustituye una matriz
completa de Steam, hardware, distribuciones, KWallet y portales reales.
