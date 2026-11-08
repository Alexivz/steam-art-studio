# Steam Art Studio 1.0.0

Primera versión oficial de **Steam Art Studio**, aplicación independiente para
personalizar el arte de Steam. No es una aplicación oficial de Valve.

## Incluye

- Biblioteca/editor, colecciones, comparación, animaciones, cola, descargas y
  copias/restauración en una sola página, sin pestañas de trabajo.
- Modo normal por defecto y confirmación antes de escribir/restaurar en Steam.
- Calidad **Original** por defecto: sin reducir resolución o fotogramas; APNG
  nativo copiado sin recodificar. Vista previa sin el tope anterior de 20 FPS.
- Descargas originales con límites independientes y errores claros; nunca se
  aplica un archivo parcial ni se reduce calidad automáticamente al fallar.

## Descargar

- **Windows x64:** ZIP portable o instalador por usuario. Extraer completo y abrir
  `steam-art-studio.exe` sin `--demo`. No se necesitan permisos de administrador.
- **Linux x86_64:** AppImage, Flatpak o DEB/RPM según la matriz de la entrega.
  AppImage requiere glibc 2.35+ y pila gráfica/fuentes/certificados del sistema.
  Flatpak requiere `org.freedesktop.Platform//26.08`; no es una publicación Flathub.
- **Fuentes:** código del proyecto y archivo separado con las fuentes de las
  dependencias, licencias y recetas correspondientes. Código propio GPL-3.0-only.

Los archivos no están firmados. Comprueba `SHA256SUMS` y revisa `VALIDACION.md`
para conocer qué se probó realmente y qué sigue pendiente. La numeración 1.0 no
significa funcionamiento universal, 60 FPS en todo equipo ni ausencia de fallos.

## Actualizar y probar

Cierra normalmente la versión anterior. No borres datos, caché, cola ni copias.
Para ZIP, extrae en otra carpeta y no abras dos versiones simultáneamente.
Para instalador Windows, desinstala primero el programa anterior: los datos están
separados. La app no cierra Steam, no autoactualiza ni aplica cambios al arrancar.

## Informar de problemas

Usa [Issues](https://github.com/Alexivz/steam-art-studio/issues/new/choose) e indica
versión, sistema, paquete, pasos y error exacto. Para animaciones puedes añadir
el enlace público del diseño. **Nunca publiques claves API, cookies, cuentas o
configuración personal**; revisa las capturas/diagnósticos antes de adjuntarlos.
Las correcciones y mejoras se publicarán como nuevas versiones.
