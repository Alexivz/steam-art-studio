# Calidad original y modo normal

## Abrir la aplicación real, sin demo

- Windows: extraer el ZIP completo y abrir **steam-art-studio.exe** directamente.
- Linux: abrir el AppImage, o `./run.sh` desde el proyecto.
- No añadir `--demo`. La pantalla muestra **MODO NORMAL** y se trabaja con la
  biblioteca real. Preparar, aplicar y restaurar mantienen sus confirmaciones.
- La versión oficial 1.0 no lleva `-preview`. Las versiones anteriores con esa
  terminación eran preliminares, no necesariamente demos. La demostración sigue
  disponible solo si se pide expresamente con `--demo`.

No se escribe en Steam al abrir, buscar o previsualizar. No se aplican cambios
automáticamente al abrir esta versión o recuperar la cola anterior.

## Mantener calidad y fluidez del original

Dejar **Animaciones al aplicar → Original · Máxima calidad**, seleccionado por
defecto. No se guarda una selección anterior de Ligera/Equilibrada como nueva
calidad predeterminada al reiniciar.

- **APNG nativo:** se aplica una copia byte por byte, sin recodificar.
- **WebP/GIF animado:** se convierte a APNG sin reducir resolución, sin recortar
  fotogramas, sin interpolación y sin compresión con pérdida adicional de los
  píxeles ya decodificados. Se conserva transparencia y temporización.
- **Equilibrada/Ligera:** son opciones voluntarias que sí reducen dimensiones/FPS.
  No se seleccionan automáticamente al fallar Original.
- Si Original supera 64 MiB de salida o 120 s de conversión, la operación se
  cancela y conserva fuente/cola; no baja calidad a escondidas. Una animación más
  corta puede encajar sin reducción. No se prometen archivos pequeños sin coste.

## Vista previa

Antes se reproducía una muestra 480×320 a 20 FPS. Desde 0.6.2, GIF/APNG/WebP se
decodifican a su tamaño nativo con temporización passthrough, sin filtro de FPS
ni escalado 480×320. El widget ajusta la imagen a su espacio visible, sin cambiar
el archivo que se aplicará a Steam. Las imágenes pequeñas tampoco se amplían al
decodificar; solo al dibujarlas si el espacio visible es mayor.

Se permite un fotograma de hasta 16 megapíxeles para limitar memoria; si lo supera,
se rechaza la reproducción, no se reduce el archivo. Los vídeos de miniatura
conservan una muestra ajustada; no son archivos aplicables a Steam. La reproducción
se detiene al ocultar la ventana, cambiar de diseño o salir del área visible.

**Sin promesas imposibles:** una animación a 12 FPS seguirá teniendo 12 FPS. No se
recupera detalle perdido antes de descargarla, ni se garantizan 60 FPS en cualquier
equipo o dentro de Steam. El sistema puede agrupar repintados si no da abasto;
esto no elimina fotogramas del archivo aplicado.

## Comprobación

Las pruebas usan un Steam sintético: APNG copiado byte por byte, WebP a 60 FPS
con igualdad de píxeles RGBA tras convertir, tamaño/recuento/duración y alfa.
También hay pruebas de vista previa nativa a 600×900/60 FPS, GIF pequeño sin
ampliación al decodificar, transparencia y parada al ocultar. Quitar el tope de
20 FPS no equivale a haber medido 60 repintados/s en todos los escritorios.

Si algo se ve distinto, distinguir reproducción dentro de Art Studio del archivo
ya aplicado dentro de Steam. Compartir el enlace público o archivo de origen y
el error concreto; nunca claves API, cookies ni datos de cuentas.
