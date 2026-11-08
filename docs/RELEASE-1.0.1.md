# Steam Art Studio 1.0.1 — Consultas Linux

Corrección del corte prematuro al buscar diseños en SteamGridDB, sin cambiar
la calidad de imágenes, la cola, las copias ni las confirmaciones de Steam.

## Qué cambia

- La API dispone de **30 s para recibir la primera respuesta** y **30 s sin
  datos**, en vez del plazo compartido de conexión de 5 s.
- La espera funciona igual con HTTPS nuevo o reutilizado: no depende de que
  Qt emita otra señal de negociación TLS.
- **Un reintento automático como máximo**, con **90 s globales por petición**
  incluyendo reintentos y redirecciones. No se reintentan errores TLS, rechazos
  de autenticación ni límites 429; se respeta Retry-After.
- Una consulta fallida ya no se presenta como «no hay diseños». Ahora se puede
  pulsar **Reintentar búsqueda** en la galería, sin ir a Descargas de originales.

## Descargar y actualizar

**Esta entrega incluye AppImage Linux x86_64 y fuentes.** Los paquetes Windows,
Flatpak, DEB y RPM siguen disponibles en [1.0.0](https://github.com/Alexivz/steam-art-studio/releases/tag/v1.0.0)
y no contienen este parche. No se ha regenerado ni validado Windows 1.0.1.

[Descargar AppImage 1.0.1](https://github.com/Alexivz/steam-art-studio/releases/download/v1.0.1/steam-art-studio-1.0.1-linux-x86_64.AppImage)
· [Todos los archivos](https://github.com/Alexivz/steam-art-studio/releases/tag/v1.0.1)

Cierra normalmente la versión anterior, sin descartar cambios pendientes.
**No borres datos, caché, cola ni copias, y no abras dos versiones a la vez.**

```sh
chmod +x steam-art-studio-1.0.1-linux-x86_64.AppImage
./steam-art-studio-1.0.1-linux-x86_64.AppImage
```

Se abre en modo normal, sin `--demo`. Para comprobar el arreglo basta buscar el
mismo juego/formato: **no hace falta aplicar imágenes a Steam**. Si falta FUSE,
añade `--appimage-extract-and-run`. Requiere glibc 2.35+, escritorio Linux,
certificados y pila gráfica; no añade soporte ARM, musl o macOS.

## Pruebas y limitaciones

- Pipeline Linux: **15 portabilidad + 6 reproducción + 70 núcleo + 38 red +
  25 interfaz + 6 empaquetado + 8 distribución**, sin fallos ni omisiones.
- Red repetida tres veces adicionales: 38/38 cada vez.
- Ubuntu 22.04/Qt 6.2.4: **CTest 6/6**. AppImage con runtime/TLS/plugins/FFmpeg
  comprobados en host y Ubuntu 22.04 sin Qt del sistema.
- Arranque normal comprobado con HOME, datos y caché aislados. Red simulada,
  claves sintéticas solo en memoria y Steam temporal durante las regresiones.
  Totales QtTest incluyen inicio/cierre; no equivalen a certificación universal.
- **Falta confirmar el resultado en la conexión del equipo que envió la captura.**
  El timeout no demuestra por sí solo la causa concreta de la lentitud.

## Fuentes y seguridad

GPL-3.0-only para el proyecto. Fuentes propias, fuentes de dependencias, licencias
y hashes acompañan la release. Se conserva FFmpeg limitado **9.0.2**, sin
GPL/nonfree/red; no se redistribuye el FFmpeg nonfree del host. Las dependencias
son las mismas verificadas para el AppImage 1.0.0.

No se ha aplicado ni restaurado arte en Steam real durante las pruebas. No se
han leído claves personales. Los paquetes no están firmados: SHA256 comprueba
integridad, no identidad. La versión 1.0.0 y sus descargas permanecen intactas.

[Reportar un fallo](https://github.com/Alexivz/steam-art-studio/issues/new/choose):
indica versión, distribución, paquete y pasos. **Nunca publiques claves API,
cookies, cuentas ni configuración personal.** Revisa las capturas antes de subirlas.
