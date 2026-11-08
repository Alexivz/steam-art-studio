# Cambios

## 1.0.1 — Corrección de consultas Linux

- API: hasta 30 s para la primera respuesta y 30 s sin datos, sin depender de
  una nueva negociación TLS. Corrige el corte de 5 s en conexiones reutilizadas.
- Un único reintento automático ante timeout/transitorios, con límite global de
  90 s por petición incluyendo reintento/redirecciones. No reintentar rechazos
  TLS, autenticación ni límites 429; se mantiene Retry-After.
- La galería distingue consulta fallida de resultado vacío e incluye
  **Reintentar búsqueda**, sin enviar al panel de descargas de originales.
- Sin cambios en calidad, cola, copias, límites de originales ni destinos Steam.
- Regresiones de primera respuesta lenta en TLS nuevo/reutilizado, inactividad,
  límite total, cancelación, reintento, seguridad y recuperación desde la GUI.
- Publicación del AppImage Linux y fuentes. Windows, Flatpak, DEB y RPM siguen
  disponibles en 1.0.0; no se presentan como paquetes corregidos 1.0.1.
- Nueva portada del repositorio con cabecera, capturas de demostración, enlaces
  directos de descarga y acceso organizado a las guías.

La captura permite identificar el timeout, no la causa concreta de la lentitud
en el equipo afectado. Falta confirmar allí con su distribución y paquete.

## 1.0.0 — Primera versión oficial de Steam Art Studio

- Lanzamiento público del proyecto independiente en GitHub Releases, bajo
  GPL-3.0-only, sin etiqueta preview y con modo normal predeterminado.
- Biblioteca/editor, colecciones, comparación, animaciones, cola, descargas y
  copias/restauración en una página continua, con confirmaciones de seguridad.
- Incorpora las correcciones de descargas 0.6.1 y la calidad Original/reproducción
  nativa sin tope fijo de 20 FPS de 0.6.2. No hay reducción automática de calidad.
- Paquetes Linux/Windows, fuentes correspondientes, avisos y SHA256; guía de
  compatibilidad y plantilla pública para recibir informes de errores.
- Se mantiene la matriz real de pruebas y las limitaciones. «Oficial» se refiere
  a Steam Art Studio, no a Valve, y no significa funcionamiento universal ni
  ausencia de errores. Las futuras correcciones tendrán nuevas versiones.

## 0.6.2-preview — Calidad original y reproducción sin tope de 20 FPS

- Vista previa GIF/APNG/WebP decodificada a tamaño nativo, con temporización
  passthrough: se elimina el escalado fijo 480×320 y el filtro de 20 FPS.
  No se amplían imágenes pequeñas al decodificar. El ajuste al widget es visual.
- Buffer de un fotograma, con límite de memoria de 16 megapíxeles. Se detiene al
  ocultar/cambiar/salir del viewport; los vídeos de miniatura conservan vista acotada.
- Original/Máxima calidad explícito y seleccionado por defecto. Sin reducción
  automática si se superan límites; Equilibrada/Ligera siguen siendo optativas.
- Modo normal identificado en pantalla; EXE/AppImage/run.sh no fuerzan demo.
  Se conserva --demo explícito para pruebas, no se lo activa por ser una preview.
- Pruebas de 60 FPS, igualdad de píxeles RGBA/alpha tras convertir, APNG aplicado
  byte por byte y vista previa nativa/transparente tanto en Linux como Windows/Wine.
- Sin prometer más FPS que el original ni fluidez universal en hardware/Steam.

## 0.6.1-preview — Descargas de animaciones

- Primer informe de un probador Windows: interfaz/aplicación de imágenes estáticas
  funcionan según el usuario; las animaciones muestran timeout en Descargas.
  No confundir ese fallo de red con conversión APNG ni con el perfil Ligera.
- Originales con plazo independiente: conexión 15 s, inactividad 30 s y total
  180 s, frente al tope total anterior de 30 s. API/miniaturas conservan sus límites.
- Temporizador de inactividad reiniciado solo por bytes reales; plazo total fijo.
  Errores distinguen conexión/inactividad/total y muestran bytes alcanzados.
- No se guardan parciales, no se amplían tamaños/dominios/concurrencia ni se
  desactiva TLS. Reintento manual conserva su función; sin reintento infinito.
- Regresiones de descarga lenta, inactividad con progreso/reintento, plazo finito,
  timeout Qt y original mayor de 64 MiB; prueba portable también en Windows/Wine.
- La misma carátula/conexión del probador todavía debe validarse con la versión nueva.

## 0.6.0-preview — Portabilidad y paquetes

- CMake/CTest/CPack multiplataforma; licencia elegida GPL-3.0-only.
- Detección Steam en Windows mediante registro/Program Files; rutas sin
  distinción de mayúsculas, rechazo de junctions/reparse points y separación
  datos/caché. KWallet solo Linux; en Windows claves únicamente en memoria.
- FFmpeg limitado 9.0.2 compilable para Linux/Windows, fuentes firmadas/pines
  SHA256, sin GPL/nonfree/red. Se necesita la versión nueva para WebP animado.
- Recetas AppImage, Flatpak, DEB/RPM, ZIP Windows e instalador NSIS por usuario.
  Fuentes/avisos e inventarios de dependencias acompañan el proceso de entrega.
- Suite portable con fixtures Unicode, restauración/conflictos, borradores,
  junctions, caché fijada, cancelación y WebP; orden natural también con locale C.
- `--runtime-check` comprueba TLS/plugins sin abrir datos ni detectar Steam.
- CI manual preparado, sin publicar releases. Estado real y limitaciones en
  `docs/PORTABILIDAD.md`; Windows nativo sigue pendiente de validación.

## 0.5.0-preview — Todo en un espacio

- Una página continua con biblioteca/editor, comparación, cola, descargas, copias
  y ajustes. Sin navegación lateral, páginas apiladas ni pestañas de formatos.
- Comparación/reproducción/preparación integradas; FFmpeg se detiene al desplazarse
  fuera del viewport. Pins conservan originales hasta limpiar/cambiar preview.
- Colecciones, conexión, asociación de títulos y diagnóstico con formularios
  integrados. Claves siempre enmascaradas y fuera de JSON/registros/diagnósticos.
- Filtros combinables, selección de cola conservada y restauración bloqueada si
  quedan cambios pendientes. Estados de operación visibles sin ventanas auxiliares.
- Se conservan confirmaciones de seguridad, selectores del sistema y formatos de
  cola/copias. Posición del nuevo separador independiente de preferencias antiguas.
- Nuevas pruebas de página única y pins, cambio de juego/formato, reproducción al
  desplazar y aplicar/cancelar/restaurar desde GUI sobre Steam sintético inyectado.
- 68 núcleo + 17 red + 24 GUI + 8 distribución, sin fallos ni omisiones.
  Sigue siendo preliminar; no implica validación con cuentas o Steam reales.

## 0.4.1-preview — Conversión de animaciones

- Errores diferenciados de tamaño APNG, timeout, arranque y conversión; no se
  registran rutas ni stderr del conversor. Comprobación del tamaño también al terminar.
- Timeout real de 120 s por conversión (antes 45 s); límite de salida de 64 MiB
  conservado. Se retiran salidas temporales incompletas y no se sobrescriben destinos.
- Perfiles explícitos en Cambios: Original por defecto, Equilibrada hasta 20 FPS
  y Ligera hasta 12 FPS, con resolución acotada y sin recortar/ampliar ni quitar alfa.
- La confirmación informa del perfil y su pérdida de resolución/FPS. Originales y
  borrador intactos; sin cambiar el formato de cola/copias ni aplicar automáticamente.
- WebP sintético de 494 fotogramas: mantiene recuento/duración en Original;
  pruebas de tamaño, timeout, transparencia, optimización/restauración e interfaz.
- 68 núcleo + 17 red + 19 GUI + 8 distribución, sin fallos ni omisiones.

## 0.4.0-preview — Rendimiento, recuperación y distribución

- Índice de biblioteca con nombres normalizados/orden natural, construido fuera
  del hilo GUI; repintados de miniaturas agrupados a 16 ms.
- Medición sintética de filtrado/ordenación con 1.000, 10.000 y 50.000 entradas;
  para 50.000, mediana anterior 36,7 ms frente a 2,9 ms indexada. No mide toda la app.
- Página Descargas con progreso, cancelación y repetición/reintento de originales.
- Red simulable, suscriptores independientes, cancelación de respuestas cacheadas,
  esperas `Retry-After` también en cola, reintentos acotados y errores de timeout.
- Limpieza segura de caché, originales fijados sin reemplazo por refrescos de URL
  y copias locales de cola identificadas por contenido.
- Borrador validado con copia anterior y JSON corrupto preservado; datos y caché
  disjuntos. Originales de la copia anterior protegidos durante limpieza normal.
- Fallos E/S inyectables solo en pruebas y worker sintético de cierre abrupto;
  restauración comprueba originales otra vez y omite archivos ya recuperados.
- FFmpeg: inicio real, error de arranque, lectura de frames acotada y parada al
  ocultar; prueba de reproducción real sobre animación sintética.
- Instalación/desinstalación por usuario con confirmación, hashes, rechazo de
  destinos ajenos/modificados y rollback; paquete reproducible del mismo binario.
- Diagnóstico JSON revisable, contadores/campos técnicos explícitos y errores de
  guardado visibles. Sin cuentas, claves, rutas, lista de juegos ni envío automático.
- Pruebas y guías de recuperación/distribución. Sigue siendo preliminar: faltan
  autenticación/KWallet reales, validación en Steam y pruebas multidistribución.
- Pase completo: 63 núcleo + 17 red + 18 GUI + 8 distribución, sin fallos ni
  omisiones. Los totales QtTest incluyen inicio/cierre de cada suite.

## 0.3.0-preview — Iconos de juegos e interfaz

- Nueva sección Icono: SteamGridDB `/icons/game/`, imágenes locales, comparación,
  preparación y cola persistente.
- ICO multirresolución: lectura de la entrada más grande, sin confundirla con
  animación. Iconos estáticos convertidos a PNG 256×256 con transparencia.
- Juegos nativos: caché `clienticon`, comprobación de metadatos y aviso de alcance
  compartido entre cuentas. Steam puede regenerar esos archivos.
- Accesos externos: icono en grid y edición precisa del campo `icon` en
  `shortcuts.vdf`, sin reescribir otros bytes; agrupación de cambios por usuario.
- Copias v2 con destinos restringidos; restauración compatible con v1 y bloqueo
  de conflictos, también si cambian otros campos de los accesos.
- Confirmación explícita de Steam cerrado para lotes con iconos.
- Iconos de acciones, categorías, reproducción y avisos; pestañas compactas sin
  títulos cortados y controles de animación desactivados para iconos.
- 51 comprobaciones de núcleo y 15 GUI; pruebas de escritura solo sintéticas.

## 0.2.0-preview — Fase 1: experiencia visual y uso

- Mantiene el diseño oscuro turquesa elegido por el usuario.
- Tema e iconos propios centralizados en `src/ui.cpp`.
- Tarjetas más legibles, marcadores de imagen discretos, tooltips y desplazamiento suave.
- Ordenación y tamaño de tarjetas persistentes; editor ocultable y separador recordado.
- Navegación compacta automática por debajo de 1260 px de ancho.
- Panel de imagen actual en ventanas de al menos 850 px de alto. En ventanas
  menores se conserva el acceso a la imagen desde Vista previa.
- Comparación de actual/nuevo, fondos para transparencias y preparación desde preview.
- Estados vacíos, selección y paginación más claros; resumen de cola sin doble título.
- Atajos de búsqueda, navegación, preparación y actualización; nombres accesibles.
- Evita un escaneo duplicado al iniciar; conserva errores al rechazar preparación.
- 34 comprobaciones de núcleo y 12 de interfaz correctas. No supone una
  certificación de estabilidad ni pruebas autenticadas con servicios reales.
- Sin cambios del formato de cola/copias, ni escrituras de prueba en Steam real.

## 0.1.0-preview — Primer prototipo

- Detección local, galería SteamGridDB, favoritos y colecciones locales.
- Caché, vista previa, animaciones, cola persistente, copias y restauración.
- Aplicación independiente en C++17/Qt 6, con confirmación antes de escribir en Steam.
