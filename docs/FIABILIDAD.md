# Rendimiento, pruebas y recuperación — 0.5.0-preview

## Alcance

Versión preliminar local para Linux. Pruebas de escritura solo con Steam
sintético en carpetas temporales. No se han aplicado/restaurado imágenes sobre
el Steam del usuario ni se ha cerrado/reiniciado ninguna instancia para probar la 0.5.

`scripts/test.sh` compila primero el ejecutable; después construye el worker de
fallos y ejecuta las suites de núcleo, red, interfaz y distribución. Registro:
`build-tests/workspace-tests.log` (pase 0.5),
`build-tests/animation-fix-tests.log` (pase 0.4.1) y
`build-tests/reliability-tests.log` (pase 0.4). Los totales QtTest incluyen inicio y cierre.

Último pase completo: **68 de núcleo, 17 de red simulada, 24 de interfaz y
8 de distribución**, sin fallos ni omisiones. Los totales incluyen inicio/cierre
de las tres suites QtTest, no son todos escenarios independientes.

Los avisos `propagateSizeHints` son del plugin Qt offscreen de pruebas; los
avisos SFINAE de GCC 16 proceden de cabeceras Qt. No se han desactivado las
comprobaciones de TLS ni utilizado conexiones de red reales para probar la red.

### Cobertura nueva

- Índice: búsqueda normalizada sin acentos, orden natural y filtros locales.
- Caché: limpieza conserva marca/archivos ajenos en raíz/copias; referencias
  contadas, originales fijados y URLs refrescadas sin reemplazar la cola.
- Borradores: límites, duplicados/rutas no seguras, hash del original local,
  copia anterior, JSON corrupto preservado y fallo al guardar la copia previa.
- Transacciones: disco lleno/permisos simulados, fallo del diario final y rollback
  sin reescribir archivos intactos. Restablecer comprueba otra vez la copia antes
  de escribir y omite destinos ya restaurados.
- Cierre abrupto: worker sintético termina tras una mutación al aplicar/restaurar;
  otro proceso recupera los originales. **No simula un corte eléctrico real** ni
  demuestra durabilidad frente a pérdida de alimentación.
- Red simulada: cancelación cacheada, suscriptores independientes, 4 solicitudes/
  2 imágenes, reintentos limitados, autenticación rechazada, fallback solo por 404,
  redirecciones no permitidas, límites de tamaño, timeout y `Retry-After` para
  nuevas peticiones y las ya en cola. Una cancelación diferida no captura el
  suscriptor de un reintento ni sigue una redirección después de cancelarse.
- Interfaz: cancelar y reintentar un original sin aplicar; reproducir GIF con
  FFmpeg real y detener al ocultar. Diagnóstico revisable antes de guardar.
- Distribución: HOME/XDG temporales; instalación, ejecución, desinstalación,
  conservación de archivos modificados y datos, rechazo de enlaces/registros
  inválidos, rollback inicial/de actualización, checksums y reproducibilidad.
  También instalación mediante el script del paquete extraído, ejecución GUI
  instalada en modo demo con captura y desinstalación mediante su comando.
- Espacio único: sin páginas/pestañas ni diálogos de trabajo auxiliares, filtros
  combinables y comparación/conexión/colecciones/diagnóstico integrados. Pruebas de
  pins durante limpieza y animaciones fuera del viewport. Flujo GUI con cancelación,
  confirmación, aplicación y restauración sobre Steam sintético inyectado.
  Ver `ESPACIO-UNICO.md`; capturas nuevas `build/workspace-*.png`.

## Medición sintética

Linux x86_64, Qt 6.10.2, compilación optimizada. Nueve consultas sobre nombres
ficticios por tamaño. Se compara la normalización/ordenación repetida anterior
con el índice reutilizable; no son mediciones de otros gestores.

| Entradas | Construir índice | Mediana anterior | Mediana indexada | RSS máximo del proceso de prueba |
| ---: | ---: | ---: | ---: | ---: |
| 1.000 | 0,97 ms | 0,426 ms | 0,017 ms | 14.652 KiB |
| 10.000 | 11,43 ms | 4,678 ms | 0,879 ms | 30.300 KiB |
| 50.000 | 78,92 ms | 36,693 ms | 2,938 ms | 97.356 KiB |

El RSS incluye los datos y ambas variantes del benchmark, no solo el índice ni
la aplicación completa. La comparación comprueba el número de resultados; el
nuevo orden natural se verifica por separado. No incluye escaneo/parsing de
Steam, decodificación de imágenes, renderizado, latencia de interfaz o red.
No es una garantía de velocidad para toda biblioteca o equipo.

Fuentes: `tests/benchmarks.cpp`, `src/libraryindex.cpp` y
`build-benchmarks/results-{1000,10000,50000}.json`. Para repetir:

```bash
mkdir -p build-benchmarks
cd build-benchmarks
qmake6 ../tests/benchmarks.pro
make -j4
./bin/studio-benchmarks 50000
```

## Recuperar una cola

1. Si guardar falla, aparece un aviso y se bloquea el cierre normal. No fuerces
   el cierre: comprueba espacio/permisos de la carpeta de datos.
2. Al abrir, un `draft.json` corrupto se renombra a
   `draft.corrupt-<identificador>.json`, sin sobrescribirlo. Se intenta leer
   `draft.previous.json` y aparece un aviso visible. Si no se puede preservar
   el archivo, no se sobrescribe con una cola nueva.
3. Se omiten entradas inseguras/duplicadas, archivos ausentes o imágenes
   inválidas; revisa el aviso y **cada cambio antes de aplicar**. La copia
   anterior puede contener cambios que ya habías retirado o aplicado.
4. El borrador contiene referencias, no una segunda copia independiente de cada
   imagen. Se fijan los originales de la cola actual y de la copia anterior para
   no borrarlos durante la limpieza normal. Borrar manualmente la caché o perder
   el disco puede impedir recuperarlos. No es una copia de seguridad externa.

No hay aplicación automática al recuperar, arrancar o cerrar. Las descargas
activas no se persisten: hay que solicitarlas otra vez; los cambios preparados sí.

## Si una animación no se convierte

El WebP/GIF comprimido puede ser pequeño y generar un APNG mucho más grande.
La 0.4.1 diferencia salida superior a 64 MiB, timeout real de 120 s, fallo de
arranque y error de FFmpeg. Verifica límites durante la conversión y al terminar;
se elimina el archivo temporal parcial, no el original ni la cola. La conversión
de todo el lote ocurre antes de las escrituras de Steam.

En **Cambios → Animaciones al aplicar**, Original conserva resolución/fotogramas.
Equilibrada o Ligera son opciones explícitas con reducción de detalle/fluidez;
no se activa una reducción automáticamente ni se recuerda entre sesiones.
Seleccionan fotogramas existentes, no interpolan ni duplican imágenes lentas;
mantienen timestamps (puede variar hasta un intervalo al reducir FPS), proporción
y transparencia. La confirmación muestra el modo antes de aplicar.

| Formato | Equilibrada (hasta 20 FPS) | Ligera (hasta 12 FPS) |
| --- | --- | --- |
| Carátula | 600×900 | 400×600 |
| Horizontal | 920×430 | 640×300 |
| Fondo | 1920×620 | 1280×414 |
| Logotipo | 1280×720 | 854×480 |

Son cajas máximas, sin recorte ni ampliación. Los iconos siguen siendo estáticos.
Si incluso Ligera excede el límite, elige otra animación; no hay garantía universal.
No se modificó ni se volvió a aplicar el arte de la cola real para probar esta
corrección. Solo se leyeron formato, tamaño, dimensiones y recuento/duración de
sus fotogramas; las conversiones/escrituras se probaron con animaciones sintéticas.

Pruebas añadidas: WebP de 494 frames/9,88 s preservado por defecto, perfiles con
resolución/FPS reducidos y restauración de originales, alfa, animación lenta sin
duplicados/ampliación, límites de tamaño/tiempo, destino existente y archivo inválido.
Captura: `build/animation-profiles.png`.

## Recuperar una operación interrumpida

1. Cierra Steam manualmente para evitar escrituras simultáneas.
2. Abre **Copias** y selecciona la operación marcada «incompleta: revisar».
3. Revisa destinos y confirma restauración; para iconos se exige confirmar
   Steam cerrado. Se comprueban hashes tanto de copias como de destinos.
4. Si hay un conflicto externo, no fuerces ni edites el manifiesto para saltarlo:
   conserva ambas versiones y revisa el caso. Especialmente `shortcuts.vdf`
   puede incluir accesos nuevos que no deben perderse.

Atomicidad por archivo, no por lote. No se promete aislamiento frente a Steam
u otros programas. Copias v1 restaurables; nuevas copias v2. Las copias están
en datos Qt, separadas de caché, y no se borran con limpieza/desinstalación.

## Diagnóstico y privacidad

En **Conexión, ajustes y privacidad → Preparar diagnóstico** se muestra el JSON
completo, de solo lectura, dentro de la página. Guardar es explícito y se informa si falla. Incluye
versión de app/Qt, sistema/arquitectura, disponibilidad de FFmpeg y contadores
de biblioteca, caché, cola y descargas. No incluye claves, identificadores de
cuenta, rutas personales, URLs, nombres de juegos ni registros. No se sube nada.

Los borradores y copias locales sí necesitan rutas/metadatos para funcionar:
no los compartas como si fueran ese diagnóstico. No se incluyen en el paquete.

## Pendiente antes de estabilidad

- Consultas autenticadas SteamGridDB y guardar/leer KWallet con participación
  del usuario; no se han leído/importado sus claves para estas pruebas.
- Validación visual de imágenes/iconos aplicados dentro de Steam, solo con
  confirmación de ese cambio concreto y copias previas.
- Medir escaneo, memoria de la aplicación completa y latencia de renderizado;
  pruebas prolongadas, matrices de Qt/distribuciones y fallos físicos reales.
- Decidir licencia, firma/verificación de procedencia y canal de publicación.
