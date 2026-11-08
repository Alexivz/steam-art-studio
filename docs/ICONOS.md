# Gestión de iconos (desde 0.3; interfaz unificada 0.5)

## Juegos de Steam

Se lee `common.clienticon` de `appinfo.vdf` (SHA-1 hexadecimal de 40 caracteres).
El archivo actual de Steam normalmente es:

```text
<Steam>/appcache/librarycache/<appid>/<clienticon>.jpg
```

Si solo existe la disposición antigua `<appid>_icon.jpg`, se usa esa ruta.
Antes de escribir se vuelven a leer los metadatos; un identificador cambiado
cancela todo el lote, sin escrituras en Steam. No se inventa un hash ni se toca
`appinfo.vdf`. Las rutas aceptadas están limitadas a esos archivos de caché.

El contenido nuevo es PNG estático, aunque conserva el nombre `.jpg` esperado
por la caché, y se escribe con archivo temporal y reemplazo atómico. Se respalda
el contenido anterior. Si antes no existía, restaurar lo elimina, siempre que
no haya sido modificado después.

**Es una sustitución de caché, no un ajuste permanente de Steam.** Afecta a todas
las cuentas que utilizan esa instalación. Steam puede descargar de nuevo el
icono oficial. No se promete persistencia frente a actualizaciones del cliente.

## Accesos externos

La imagen nueva se guarda como:

```text
<Steam>/userdata/<usuario>/config/grid/<appid>_icon.png
```

Además se debe actualizar el campo `icon` del acceso en `shortcuts.vdf`.
El editor binario propio cambia únicamente el valor de ese campo, o lo añade si
falta. Mantiene literalmente los demás bytes: orden, mayúsculas, campos futuros,
opciones de lanzamiento, etiquetas y otros accesos. No ejecuta esos valores.

Se rechazan archivos truncados, campos ambiguos, AppID duplicados y estructuras
no soportadas. Varios iconos del mismo usuario se agrupan en una única edición
de `shortcuts.vdf` dentro de la transacción.

## Formatos y vista previa

- En 0.5 se elige Icono en el desplegable de formato. La comparación y preparación
  están integradas en la página, sin abrir otra ventana.
- PNG, ICO, JPEG, WebP y GIF estático de entrada, hasta los límites de imagen.
- Se rechazan iconos animados; un ICO con varias resoluciones no es una animación.
- Para ICO se elige la resolución más grande segura entre un máximo de 256 entradas.
- Salida: PNG de 256×256, centrado, sin deformar y con relleno transparente.
- Lectura y miniaturas de ICO usan la misma elección de resolución que la conversión.
- Comparación con el icono actual, fondos para transparencia y preparación desde preview.

## Confirmación, copias y conflictos

No se escribe al explorar, buscar o preparar. Aplicar/restaurar iconos requiere
confirmación explícita y marcar que Steam está cerrado. Art Studio no comprueba
todos los procesos ni cierra Steam por su cuenta.

Los manifiestos nuevos usan versión **2**, con destinos limitados por ámbito:
`grid`, `library-icon` y `shortcuts`. Se siguen restaurando manifiestos v1.
Hay copias de las imágenes sustituidas y del `shortcuts.vdf` completo. Este puede
contener rutas personales y opciones de lanzamiento; consérvalo en la carpeta
privada de copias, no lo publiques como diagnóstico.

Los hashes se comprueban antes de escribir y restaurar. Si cualquier parte de
`shortcuts.vdf` cambió después, se bloquea la restauración completa del lote para
no eliminar, por ejemplo, juegos recién añadidos. Una interrupción o fallo de E/S
puede dejar un lote parcial: la escritura es atómica por archivo, no por lote.
No hay garantía frente a modificaciones simultáneas entre la comprobación y la
escritura; por eso se pide cerrar Steam.

No modifica accesos `.desktop`, iconos del escritorio/sistema, ejecutables,
licencias o datos de cuenta. Cambiar el icono de Steam no cambia necesariamente
el icono de la ventana del juego en el escritorio Linux.

## Verificación y referencias

Las pruebas usan directorios temporales y VDF/ICO sintéticos. Se verifican las
rutas, conversión, edición por bytes, agrupación, copias, conflictos, restauración
y controles GUI. No se han aplicado iconos al Steam real del usuario ni se han
hecho búsquedas autenticadas para esta entrega.

Se contrastó la disposición del cliente con la implementación pública de
[Decky SteamGridDB](https://github.com/SteamGridDB/decky-steamgriddb/blob/main/main.py)
y con la presencia de archivos `clienticon` en lectura local. El código de
edición/transacción de Art Studio es propio; no usa Decky ni su clave.
Formatos de la comunidad: [FAQ de SteamGridDB](https://www.steamgriddb.com/faq).
