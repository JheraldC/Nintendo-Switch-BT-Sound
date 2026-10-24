# Copias y reversión

El asistente guarda archivos privados fuera de la SD, normalmente en `Documentos/SwitchBackups/jherald-FECHA-HORA`. Conserva esa carpeta; no la subas a GitHub.

Para sonido o logo, `BACKUP.json` identifica cada ruta y su hash anterior. Un valor `null` significa que el archivo no existía. Con la consola apagada, restaura los archivos anteriores de esa carpeta en las mismas rutas; elimina únicamente los archivos nuevos que el manifiesto marque como inexistentes antes. Si cambiaste después el menú, combina la opción `logopath=` con cuidado en lugar de restaurar un menú antiguo entero.

Para desactivar solo el sonido, retira `atmosphere/contents/42000000004A4253/flags/boot2.flag`. No borres otras carpetas de módulos.

Bluetooth guarda su plan, configuración y base anterior en el respaldo privado. `rollback.py` es una herramienta histórica de reversión del menú: en esta edición manual no hace cambios porque nunca se editó ese menú; no restaura emparejamientos. Para retirar una instalación nueva sin sincronizaciones, elimina solo los cuatro archivos añadidos indicados en su plan. Para restaurar emparejamientos después de usar el payload, revisa y restaura base e historial del mismo punto de respaldo de esa consola; no combines una base anterior con un historial nuevo ni con una transacción pendiente. Si hay una transacción interrumpida, intenta primero su recuperación con el mismo payload antes de arrancar emuMMC. Conserva los archivos actuales antes de una restauración manual.

El instalador de sonido/logo verifica cada archivo y usa reemplazo temporal, pero una operación de varios archivos no es atómica frente a una desconexión física. Si falla, conserva el mensaje y el respaldo; no borres la copia. El respaldo externo es necesario también para cambios manuales de `package3`.
