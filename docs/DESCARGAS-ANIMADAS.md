# Si una animación falla en Descargas

## Identificar la fase

- **Descargas → Error / tiempo de espera:** falta descargar el original desde
  SteamGridDB. Todavía no se ha convertido ni aplicado esa animación.
- **Cambios preparados → error APNG / FFmpeg / 64 MiB / 120 s:** el original ya
  llegó; el fallo pertenece a la conversión. Aquí sí puede ayudar el perfil Ligera.
- La miniatura y el original son archivos distintos: ver una miniatura animada
  no demuestra que el original ya se haya descargado.

## Corrección 0.6.1

En 0.6.0, los originales compartían un plazo total de 30 s con API/miniaturas,
incluso si estaban recibiendo datos. 0.6.1 usa para originales:

| Límite | Valor |
| --- | --- |
| Inicio de conexión/respuesta | 15 s |
| Sin recibir bytes | 30 s |
| Total de una petición, aunque lleguen bytes | 180 s |
| Tamaño máximo de entrada | 64 MiB |

Los errores distinguen estos tiempos y conservan la cantidad descargada antes
del fallo. No se cachea ni se aplica un archivo parcial. **Reintentar** inicia
otra descarga completa; no es una reanudación por rangos.
No se eliminan límites, no se desactiva TLS ni se envían claves al CDN.

## Qué pedir al probador

1. Cerrar Art Studio normalmente. Extraer el ZIP actual completo en otra carpeta
   y abrir 1.0.0. No borrar datos, caché ni copias; no abrir dos versiones a la vez.
2. Reintentar la misma carátula desde Descargas. Si funciona, revisar/preparar y
   confirmar como siempre; no se aplica nada automáticamente.
3. Si falla, enviar **el texto exacto del error**, los bytes alcanzados, versión
   de la app y el **enlace público del diseño** o el archivo de animación.
   Nunca enviar la clave API, JSON de configuración, cookies o cuentas Steam.
4. Como alternativa, descargar esa imagen desde la web de SteamGridDB y usar
   **Imagen local…** en Art Studio. Esto permite separar un problema de descarga
   de un problema de conversión. No demuestra que el fallo de red esté resuelto.

La corrección se comprueba con respuestas sintéticas lentas, bloqueadas y de
tamaño excesivo. Sigue pendiente reproducir el diseño y conexión concretos del
probador; la captura original por sí sola no permite garantizar la causa única.
