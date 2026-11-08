# Instalador local Linux y archivo dinámico

Esta guía describe el instalador local conservado desde las primeras entregas.
Para AppImage, Flatpak, DEB/RPM y Windows 1.0, ver [PORTABILIDAD.md](PORTABILIDAD.md).

## Ejecutar sin instalar

El archivo dinámico creado con `./scripts/package.sh` contiene un
binario dinámico, icono, documentación, instalador Python y lanzadores relativos.
No contiene Steam, credenciales, configuración, cola, caché ni copias del usuario.

Extrae en una carpeta nueva, entra en ella y ejecuta:

```bash
sha256sum -c SHA256SUMS
./run.sh
```

Necesita Linux x86_64 y bibliotecas de sistema/Qt compatibles con la compilación
(aquí Qt 6.10.2, módulos Core/Gui/Widgets/Network/DBus y plugins de plataforma e
imagen). FFmpeg es opcional para reproducción/conversión animada. Python 3 no
es necesario para abrir la aplicación; sí para instalar/desinstalar. El código
fuente requiere además Qt Concurrent, herramientas qmake y compilador C++17.

**No es autocontenido ni un AppImage universal.** Las dependencias resuelven en
este equipo y el binario extraído se ha ejecutado aquí; no es una prueba en
otras distribuciones. Consulta `RUNTIME.txt` del paquete si faltan bibliotecas.

## Instalar para tu usuario

Desde el paquete: `./install.sh`. Desde el proyecto compilado:
`./scripts/install.sh`. Ambos piden escribir `SI`; no uses `sudo`.

| Elemento | Ubicación |
| --- | --- |
| Programa y desinstalador | `$XDG_DATA_HOME/steam-art-studio-app/` |
| Comandos | `~/.local/bin/steam-art-studio` y `steam-art-studio-uninstall` |
| Menú de aplicaciones | `$XDG_DATA_HOME/applications/steam-art-studio.desktop` |
| Icono | `$XDG_DATA_HOME/icons/hicolor/scalable/apps/steam-art-studio.svg` |

Por defecto `XDG_DATA_HOME` es `~/.local/share`. Debe ser una ruta absoluta.
No modifica variables de entorno ni el PATH. Si `~/.local/bin` no está en tu
PATH, utiliza la ruta completa. El menú puede necesitar recargar su caché.

El instalador comprueba `--version` antes de escribir. Rechaza destinos ajenos,
enlaces simbólicos y archivos modificados; mantiene manifiesto con hashes y
rollback de las escrituras si ocurre un error detectable. No cierra instancias
abiertas ni hace migraciones de credenciales o cambios de Steam.

## Actualizar y desinstalar

Cierra tú la aplicación cuando quieras utilizar la versión nueva; compilar o
instalar no actualiza ventanas abiertas. Repite el instalador desde la nueva
versión para actualizar archivos del programa previamente identificados.

```bash
~/.local/bin/steam-art-studio-uninstall
```

Usa el mismo usuario, HOME y XDG_DATA_HOME de la instalación. Si se cambian,
el registro no se reconocerá: no se buscan ni borran otras ubicaciones.
Si algún archivo ha sido personalizado, la desinstalación se cancela antes de
borrar el resto; conserva la personalización y el manifiesto para revisión.
No se hace borrado recursivo de carpetas; archivos desconocidos se conservan.

**Los datos Qt, cola, caché y copias no se desinstalan.** Están separados del
programa. Abre su carpeta desde Conexión, ajustes y privacidad. No borres la caché manualmente
si necesitas originales de la cola; ver `FIABILIDAD.md`.

Las escrituras del instalador son atómicas por archivo; no se promete una
transacción única frente a corte eléctrico, instaladores concurrentes o cambios
externos durante la operación. Las pruebas de rollback usan fallos simulados.

## Crear el archivo local

```bash
./scripts/build.sh
./scripts/test.sh
./scripts/package.sh
```

La versión de `--version` debe coincidir con `VERSION`. El archivo contiene un
`SHA256SUMS` de todos sus archivos salvo el propio listado. El comando imprime
además el SHA256 del `.tar.gz`; estos hashes detectan alteraciones, pero no son
una firma ni prueban quién produjo el archivo.

Para no sobrescribir una entrega existente usa otra salida:

```bash
./scripts/package.sh --output /tmp/opencode/studio-verificacion.tar.gz
```

El empaquetador usa una lista explícita de archivos, orden estable y uid/gid/
fechas normalizados. `SOURCE_DATE_EPOCH` fija la fecha interna (por defecto 0).
Reproducibilidad comprobada con **el mismo binario, documentación, herramientas
y entorno**, no entre distintos compiladores/distribuciones o versiones Python.
No se incluyen bibliotecas de terceros. Código propio GPL-3.0-only. Sin firma,
publicación ni instalación nueva en el perfil real del usuario.
