# Una página, todas las herramientas — 0.5

La 0.5 reúne las mejoras anteriores en una sola entrega y una página continua.
Se recorre verticalmente; no se intenta comprimir todo en una vista de 720 px.
No hay pestañas, páginas alternativas, acordeones que haya que desplegar ni
ventanas de trabajo auxiliares. La biblioteca y el editor están juntos arriba.

| Antes | Ahora |
| --- | --- |
| Biblioteca/Favoritos/Colecciones en navegación lateral | Búsqueda y filtros combinables sobre la misma biblioteca |
| Pestañas de formato | Desplegable de formato, con icono y nombre |
| Ventana de vista previa/comparación | Panel actual/nuevo, reproducción y preparación integrados |
| Página Cambios | Panel Cambios preparados bajo la comparación |
| Página Descargas | Panel de originales con progreso, cancelar y reintentar |
| Página Copias | Panel de historial/restauración, junto a Descargas o debajo en ventanas estrechas |
| Ventanas de crear/añadir colección | Nombre, destino y botones visibles sobre la biblioteca |
| Ventanas de conexión y asociación manual | Campos y selección de coincidencia en Conexión, ajustes y privacidad |
| Ventanas de configuración/diagnóstico | Controles y JSON de diagnóstico revisable en la página |

**Vista previa**, **Ver actual** y **Ajustes** solo desplazan la página hasta su
panel. Los atajos también enfocan/desplazan; nunca aplican imágenes. Buscar
otro título o crear una colección no abre una ventana nueva.

La animación se inicia a petición y se detiene si sale del área visible. La
comparación fija sus originales mientras se utiliza; limpiar comparación libera
esas referencias. Preparar conserva una copia independiente de la descarga.

Cambiar de juego/formato descarta la propuesta de preview anterior, no la cola.
Las colecciones y favoritos son locales, no una sincronización de Steam.

## Excepciones deliberadas

- Confirmar aplicar/restaurar y, para iconos, Steam cerrado.
- Confirmar vaciar cola, limpiar caché o importar una clave del gestor anterior.
- Elegir archivos/carpetas y guardar el diagnóstico con el selector del sistema.
- Seguir un enlace explícito a SteamGridDB o abrir la carpeta de datos.

Son acciones de seguridad/sistema, no pestañas o pantallas de trabajo adicionales.
No se cambian los originales de Steam sin confirmación y copias.

## Verificación

Las pruebas GUI comprueban ausencia de QTabWidget/QStackedWidget y diálogo modal
en el flujo normal, persistencia de cola/colecciones/favoritos, tamaños 1024×720
y 1480×940, control de animación al desplazarse y protección de originales.
La aplicación/restauración real del flujo GUI se prueba con un Steam sintético
inyectado, sin descubrir Steam real ni usar credenciales o red real.

Capturas revisadas en `build/workspace-*.png`. El `.png` complete representa
el contenido continuo; la ventana real muestra una parte y permite desplazarlo.
No es una validación autenticada ni de arte aplicado dentro de Steam real.
