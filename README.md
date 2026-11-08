<div align="center">

![Steam Art Studio: Tu biblioteca. Tu estilo.](docs/media/hero.svg)

# Steam Art Studio

**Dale otra imagen a tu biblioteca de Steam.**

Carátulas, fondos, logotipos, iconos y animaciones.<br>
Una aplicación de escritorio independiente. Todo en un mismo espacio.

[![Linux AppImage 1.0.1](https://img.shields.io/badge/Linux-AppImage_1.0.1-70e2cb?style=for-the-badge&labelColor=14212b)](https://github.com/Alexivz/steam-art-studio/releases/tag/v1.0.1)
[![Windows x64 1.0.0](https://img.shields.io/badge/Windows-x64_1.0.0-a8bdd2?style=for-the-badge&labelColor=14212b)](https://github.com/Alexivz/steam-art-studio/releases/tag/v1.0.0)
[![Licencia GPL-3.0-only](https://img.shields.io/badge/Licencia-GPL--3.0--only-70e2cb?style=for-the-badge&labelColor=14212b)](LICENSE)

[Descargas](#descargas) · [Así se ve](#así-se-ve) · [Primeros pasos](#primeros-pasos) · [Documentación](#documentación) · [Reportar un fallo](https://github.com/Alexivz/steam-art-studio/issues/new/choose)

</div>

> **Nuevo en 1.0.1 · Corrección Linux:** las consultas a SteamGridDB ya no se
> cortan a los 5 segundos. Se distingue un error de un resultado vacío y se añade
> **Reintentar búsqueda**. [Ver cambios y pruebas →](docs/RELEASE-1.0.1.md)

## Pensado para trabajar sin saltar entre pestañas

| Explora | Compara | Decide |
| :--- | :--- | :--- |
| Biblioteca, favoritos y colecciones locales. | Imagen actual y diseño nuevo, juntos. | Prepara una cola y confirma antes de aplicar. |
| Diseños de SteamGridDB o imágenes locales. | Animaciones con calidad Original por defecto. | Copias restaurables y comprobación de conflictos. |

## Así se ve

![Biblioteca y editor de Steam Art Studio en una pantalla continua](docs/media/workspace.png)

<p align="center"><sub>Captura en modo demostración: juegos y diseños sintéticos, sin cuentas ni claves personales. Los paquetes arrancan en modo normal.</sub></p>

<details>
<summary><strong>Ver la comparación antes de preparar un cambio</strong></summary>

![Comparación de una imagen actual y un diseño nuevo, todavía sin aplicar](docs/media/comparison.png)

La vista previa no aplica imágenes. Preparar tampoco: la escritura en Steam
requiere una confirmación explícita y conserva una copia restaurable.

</details>

## Descargas

| Tu sistema | Paquete | Versión |
| :--- | :--- | :--- |
| **Linux x86_64** | **[AppImage · descargar](https://github.com/Alexivz/steam-art-studio/releases/download/v1.0.1/steam-art-studio-1.0.1-linux-x86_64.AppImage)** | **1.0.1 · incluye la corrección de consultas** |
| Windows 10/11 x64 | [ZIP portable](https://github.com/Alexivz/steam-art-studio/releases/download/v1.0.0/steam-art-studio-1.0.0-windows-x86_64.zip) · [Instalador por usuario](https://github.com/Alexivz/steam-art-studio/releases/download/v1.0.0/steam-art-studio-1.0.0-windows-x86_64-setup.exe) | 1.0.0 |
| Linux · otros formatos | [Flatpak, DEB y RPM](https://github.com/Alexivz/steam-art-studio/releases/tag/v1.0.0) | 1.0.0 · no incluyen el parche 1.0.1 |
| Código y licencias | [Fuentes y SHA256](https://github.com/Alexivz/steam-art-studio/releases/tag/v1.0.1) | 1.0.1 |

**Requisitos:** AppImage necesita glibc 2.35+ y un escritorio Linux con su pila
gráfica y certificados. Windows se ha comprobado bajo Wine; no equivale a una
certificación nativa completa. Consulta la [matriz de compatibilidad](docs/PORTABILIDAD.md).
Los paquetes no están firmados. Comprueba los hashes antes de ejecutar.

## Primeros pasos

1. **Abre** el AppImage o extrae el ZIP completo y abre `steam-art-studio.exe`.
2. **Elige un juego y formato.** Puedes usar una imagen local sin clave o cargar
   tu clave SteamGridDB dentro de la aplicación para buscar diseños.
3. **Compara y prepara.** Revisa la cola y confirma solo cuando quieras aplicar.

```bash
# Linux: ejecutar sin instalar y sin --demo
chmod +x steam-art-studio-1.0.1-linux-x86_64.AppImage
./steam-art-studio-1.0.1-linux-x86_64.AppImage
```

¿Falta FUSE? Añade `--appimage-extract-and-run`. Para actualizar, cierra
normalmente la versión anterior: **no borres datos, cola, caché ni copias**.

## Lo importante, sin letra pequeña

- **Original por defecto.** Sin reducción automática de calidad al fallar.
- **Tú confirmas.** Arrancar, explorar y previsualizar no escriben en Steam.
- **Tu clave no va al repositorio.** Solo memoria o KWallet en Linux; nunca JSON
  ni diagnósticos. No se envían informes automáticamente.
- **Pruebas, no promesas universales.** El parche pasó 38 pruebas de red, 25 de
  interfaz y el resto del pipeline local; Ubuntu CTest 6/6. Falta confirmar la
  conexión concreta del equipo que reportó el fallo. [Validación →](docs/RELEASE-1.0.1.md)

## Documentación

| Quiero… | Guía |
| :--- | :--- |
| Elegir paquete o compilar | [Compatibilidad y distribución](docs/PORTABILIDAD.md) |
| Entender animaciones y calidad | [Calidad y modo normal](docs/CALIDAD-Y-MODO-NORMAL.md) · [Descargas](docs/DESCARGAS-ANIMADAS.md) |
| Conocer las protecciones | [Fiabilidad y recuperación](docs/FIABILIDAD.md) · [Iconos](docs/ICONOS.md) |
| Revisar novedades | [Historial de cambios](CHANGELOG.md) |
| Informar de un problema | [Abrir una incidencia](https://github.com/Alexivz/steam-art-studio/issues/new/choose) |

<details>
<summary><strong>Guía completa: biblioteca, atajos, animaciones, compilación y datos</strong></summary>


## Una sola pantalla de trabajo

No hay pestañas, barra lateral de páginas ni ventanas auxiliares para trabajar:
todo está en una página continua con **desplazamiento vertical**.

- Biblioteca y editor juntos: búsqueda, orden natural, tamaños de tarjeta,
  favoritos y colecciones locales, siempre en el mismo espacio.
- Formato mediante un desplegable: Carátula, Horizontal, Fondo, Logotipo e Icono.
- Comparación actual/nuevo integrada, fondos para transparencia y reproducción
  opcional con FFmpeg. Preparar desde la comparación no aplica a Steam.
- Cola, perfiles de animación, descargas con progreso/cancelar/reintentar y
  copias restaurables bajo el editor, sin cambiar de página.
- Colecciones, conexión, asociación manual de títulos, mantenimiento y diagnóstico
  con formularios integrados, sin tener que abrir diálogos de configuración.
- Errores de aplicación/restauración visibles en la propia pantalla.

**Se conservan las confirmaciones de seguridad** antes de aplicar/restaurar,
vaciar la cola o limpiar caché. Elegir archivos/carpetas y guardar el diagnóstico
utilizan el selector del sistema. Los enlaces explícitos pueden abrir el navegador.
No se eliminan esas protecciones por simplificar la interfaz.

## Abrir

Desde la carpeta del proyecto:

```bash
./run.sh
```

`run.sh` compila solo si falta el binario. Después de modificar código, recompila
con `./scripts/build.sh`. No actualiza, cierra ni reinicia instancias abiertas.

**Modo normal por defecto**, con biblioteca real y aplicación de cambios tras
confirmación. En Windows abre directamente `steam-art-studio.exe`; en Linux el
AppImage o `./run.sh`, **sin `--demo`**. La demo solo se activa si añades expresamente
`--demo`; no es el modo predeterminado de los paquetes oficiales.

## Flujo de trabajo

1. Busca un juego. Puedes combinar **Solo favoritos**, estado y colección sin
   salir de la biblioteca. Las colecciones y favoritos solo pertenecen a Art Studio.
2. Elige formato y usa **Imagen local…**, o carga tu clave desde **Conexión,
   ajustes y privacidad** para consultar SteamGridDB.
3. Selecciona un diseño y pulsa **Vista previa**: te lleva a la comparación de
   esta misma página. Reproduce la animación solo si lo solicitas. Se detiene
   al cambiar de diseño, ocultar la ventana o dejarla fuera del área visible.
4. Pulsa **Preparar** o **Preparar este diseño**. Revisa el panel **Cambios
   preparados**, el perfil de animación y los destinos antes de confirmar Aplicar.
5. **Copias y restauración** permite recuperar los originales con verificación
   de hashes/conflictos. La cola debe estar vacía para restaurar.

Para corregir una asociación, escribe otro título en el formulario de SteamGridDB
dentro de Ajustes, selecciona una coincidencia y pulsa **Asociar título**. No se
ejecutan juegos ni comandos de los metadatos.

Los atajos desplazan/enfocan la misma página: **Ctrl+F** buscar, **Ctrl+1**
biblioteca, **Ctrl+2** alternar favoritos, **Ctrl+3** colección, **Ctrl+4** cola,
**Ctrl+5** copias, **Ctrl+6** descargas, **Ctrl+Intro** preparar y **F5** actualizar.
Ningún atajo aplica cambios a Steam.

## Animaciones

Las animaciones se descargan completas antes de convertirlas. Desde 0.6.1 los
originales tienen hasta **180 s en total**, **30 s sin recibir datos** y **15 s
para iniciar conexión**, en lugar del tope total de 30 s compartido con miniaturas.
El límite de entrada sigue siendo 64 MiB. Si falla, Descargas conserva el progreso
alcanzado y permite Reintentar; una descarga parcial nunca se aplica ni se cachea.
Como alternativa, descarga el archivo desde SteamGridDB en el navegador y usa
**Imagen local…**. El perfil Ligera afecta a la conversión, no acelera la descarga.

Un WebP/GIF pequeño puede producir un APNG mucho mayor. Los errores distinguen
salida superior a **64 MiB**, timeout real de **120 s** y fallo de FFmpeg.
Los temporales incompletos se eliminan; el original y la cola se conservan.

En **Cambios preparados → Animaciones al aplicar**:

- **Original · Máxima calidad**, por defecto: conserva resolución, fotogramas,
  duración y transparencia. APNG nativo se copia sin recodificar; GIF/WebP se
  convierten a APNG sin pérdida adicional de los píxeles decodificados.
- **Equilibrada**: hasta 20 FPS y dimensiones acotadas por formato.
- **Ligera**: hasta 12 FPS; carátulas hasta 400×600.

La reducción es explícita, aparece en la confirmación y solo afecta a la copia
aplicada. Mantiene proporciones y transparencia, sin ampliar, recortar ni interpolar.
Puede reducir detalle/fluidez. Si incluso Ligera excede los límites, elige otra
animación más corta/sencilla; no se garantiza que cualquiera vaya a caber.
Detalles y recuperación: [docs/FIABILIDAD.md](docs/FIABILIDAD.md).

Desde 0.6.2, la comparación reproduce GIF/APNG/WebP a resolución nativa y con
temporización passthrough, sin el recorte anterior a 480×320/20 FPS. El ajuste al
tamaño visible solo es de pantalla: no modifica el archivo. No se inventan
fotogramas ni se promete transformar una animación lenta en 60 FPS. La fluidez
real depende del original, del equipo y de Steam. La vista previa limita memoria
a 16 megapíxeles por fotograma; superarlo cancela la reproducción, no reduce el
archivo. Los vídeos de miniatura mantienen una vista ajustada y no son aplicables.

## Iconos

Entrada PNG/ICO y otras imágenes estáticas; ICO multirresolución usa la imagen
mayor segura. Salida PNG transparente 256×256, sin deformar.

- Juegos nativos: caché `clienticon`, **compartida entre cuentas** de la instalación.
  Steam puede regenerarla. Se verifican los metadatos actuales antes de escribir.
- Externos: imagen en `grid` y edición precisa del campo `icon` de `shortcuts.vdf`,
  conservando sus demás bytes. El VDF completo se respalda y los cambios externos
  posteriores bloquean una restauración que pudiera perder accesos nuevos.
- Aplicar/restaurar iconos requiere confirmar Steam cerrado; Art Studio no lo cierra.

No cambia iconos del escritorio, ejecutables ni archivos `.desktop` de juegos.
Rutas y límites: [docs/ICONOS.md](docs/ICONOS.md).

## Compilación y pruebas

Requiere compilador C++17 y Qt 6 Core/Gui/Widgets/Network/Concurrent/Svg.
Se puede usar CMake 3.22+/Ninja o qmake6. DBus/KWallet es opcional y solo Linux.
FFmpeg es opcional para reproducción/conversión de animaciones.

```bash
./scripts/build.sh
./scripts/test.sh
./run.sh --demo
```

Las suites cubren núcleo, red simulada, GUI, portabilidad y distribución.
Los totales QtTest incluyen inicio/cierre de cada suite. El pipeline compila la
aplicación antes de probar. Matriz real y registros: [docs/PORTABILIDAD.md](docs/PORTABILIDAD.md).

La GUI comprueba ausencia de pestañas/páginas, formularios integrados, persistencia,
cancelación/reintento, originales fijados en la comparación y parada al desplazarse.
También aplica/restaura a través de la interfaz con confirmación sobre un **Steam
sintético inyectado**, sin detectar el Steam real en esa prueba. Las pruebas de red
no usan sockets ni claves reales. El modo demo desactiva aplicar y restaurar.

Capturas sintéticas: `build/workspace-desktop.png`, `workspace-compact.png`,
`workspace-preview.png`, `workspace-queue.png`, `workspace-downloads.png`,
`workspace-settings.png` y `workspace-complete.png` (contenido continuo completo).

Filtrado/ordenación sintéticos con 50.000 entradas: mediana anterior **36,7 ms**
frente a **2,9 ms** con índice. No mide toda la aplicación, escaneo, renderizado,
red u otros gestores. Medidas y límites en `docs/FIABILIDAD.md`.

## Instalación y formatos

Para la mayoría de usuarios Linux, el objetivo es **AppImage** o **Flatpak**;
para Windows, **ZIP portable** o **instalador por usuario**. También hay paquetes
**DEB/RPM** dinámicos. Consulta artefactos, requisitos, pruebas y límites en
[docs/PORTABILIDAD.md](docs/PORTABILIDAD.md). No se garantiza «cualquier distro»:
AppImage requiere Linux x86_64/glibc 2.35+; Flatpak necesita su runtime.
Windows se comprueba aquí mediante Wine, no sustituye una prueba Windows nativa.

El ZIP es portable en cuanto al programa, **no guarda tus datos al lado del EXE**.
En Windows los datos y caché se separan dentro de la ubicación local Qt; las
claves permanecen solo en memoria. No se pide administrador para el instalador.

También se conserva el instalador Linux local de las entregas anteriores:

```bash
./scripts/install.sh   # pide confirmación, sin sudo
./scripts/package.sh   # crea un tar.gz dinámico de la versión actual
```

Después de instalar: `~/.local/bin/steam-art-studio`. Desinstalar:
`~/.local/bin/steam-art-studio-uninstall`. Se conservan datos, cola, caché y copias;
no se sobrescriben archivos ajenos/modificados ni se reinicia la aplicación.

El paquete extraído incluye `run.sh`, `install.sh`, documentación y `SHA256SUMS`.
Ese tar.gz local es **dinámico Linux x86_64**: necesita Qt y plugins compatibles
con el compilador utilizado. No confundirlo con el AppImage de Releases. Python 3
solo se necesita para ese instalador local. Reproducible con los mismos archivos/
binario, no entre distintos compiladores o distribuciones.
Código propio **GPL-3.0-only**; ver `LICENSE` y `THIRD_PARTY_NOTICES.md`.
Las actualizaciones se publican en GitHub Releases; no hay autoactualización ni
firma de código. Ver `docs/DISTRIBUCION.md`.

## Datos y seguridad

- Explorar, previsualizar, preparar y arrancar no escriben en Steam. No hay
  aplicación automática al cerrar, restaurar una cola o recuperar un borrador.
- Cola: 100 cambios/256 MiB de originales; preparación total: 512 MiB.
- Caché: presupuesto 512 MiB, con referencias para cola, copia anterior del borrador
  y comparación. Los archivos fijados pueden superar temporalmente ese presupuesto.
- Datos y copias están en la ubicación Qt de `SteamArtStudio/steam-art-studio`,
  separados de caché y del programa instalado. **Abrir datos y copias** muestra
  esa carpeta. No contiene una copia de toda tu biblioteca Steam.
- `--data-dir` y `--cache-dir` requieren carpetas dedicadas, vacías o marcadas por
  Art Studio. No pueden ser iguales ni contenerse. No borres caché manualmente
  si necesitas los originales de la cola.
- Escritura atómica por archivo, **no por lote**. Un cierre abrupto puede dejar
  una operación incompleta recuperable desde Copias. No se garantiza durabilidad
  frente a cortes eléctricos ni aislamiento frente a otros programas: cierra Steam.
- Claves solo en memoria/KWallet. Importar la del gestor anterior es una acción
  separada y confirmada; no se hace por iniciativa del agente ni al arrancar.
- El diagnóstico integrado usa campos técnicos/contadores, sin claves, cuentas,
  rutas, URLs o listas de juegos. No se envía nada automáticamente.

La matriz distingue pruebas Linux, Windows bajo Wine e informes de usuarios.
La autenticación real SteamGridDB/KWallet, el resultado visual en todas las
versiones de Steam y la fluidez en otros equipos no se garantizan por el número 1.0.
Ver `CHANGELOG.md` y [limitaciones y pruebas](docs/PORTABILIDAD.md).

## Informar de problemas

Abre una [incidencia](https://github.com/Alexivz/steam-art-studio/issues/new/choose)
indicando versión, sistema, paquete, pasos y error exacto. Para animaciones, añade
el enlace **público** del diseño si puedes. Revisa las capturas y diagnósticos:
no publiques claves API, cookies, cuentas ni configuración personal. No se envían
informes automáticamente. Conserva las copias y evita seguir aplicando cambios
si detectas un conflicto hasta revisarlo.

</details>

---

<div align="center">

Hecho por [Alexivz](https://github.com/Alexivz) · C++17 / Qt 6 · [GPL-3.0-only](LICENSE)

<sub>Proyecto independiente. No es una aplicación oficial de Valve ni reemplaza Steam Art Manager.<br>Las marcas y los diseños de terceros pertenecen a sus respectivos propietarios.</sub>

</div>
