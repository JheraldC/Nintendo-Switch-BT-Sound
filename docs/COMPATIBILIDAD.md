# Perfiles Bluetooth: conservar y ampliar

Las versiones anteriores permanecen en Releases. El perfil `nintendo` conserva el filtro anterior de Pro Controller y Joy-Con. El usuario ejecuta un único `BT-Sync-Una-Vez.bin`. La reparación respalda el binario anterior en PC y actualiza ese archivo, conservando su continuación, historial y emparejamientos; no reinicia la base para hacerla pasar.

## Mandos

| Perfil | Registros que puede seleccionar | Estado |
|---|---|---|
| `nintendo` | Pro Controller, Joy-Con (L), Joy-Con (R), con la identificación y clase HID que ya se admitían | Ruta anterior conservada |
| `missioncontrol` | Los anteriores, más candidatos Sony DualShock 4, DualSense/Edge y Xbox One S Bluetooth clásico de la tabla siguiente | Opcional y experimental; exige MissionControl habilitado |

Algunos mandos de otras marcas en modo Switch se presentan como `Pro Controller` y ya pueden pasar el filtro anterior si su registro cumple todos los campos requeridos. Eso no prueba automáticamente cada marca o modelo.

El perfil ampliado utiliza coincidencias exactas de fabricante/producto y nombre, además de clase HID, indicador de emparejamiento y clave de enlace no vacía:

| Fabricante/producto | Nombre admitido |
|---|---|
| Sony `054c:05c4`, `054c:09cc` | `Wireless Controller` |
| Sony `054c:0ce6` | `DualSense Wireless Controller` o `Wireless Controller` |
| Sony `054c:0df2` | `DualSense Edge Wireless Controller` |
| Microsoft `045e:02e0`, `045e:02fd` | `Xbox Wireless Controller` |

Identificadores tomados de los encabezados oficiales de [DualShock 4](https://github.com/ndeadly/MissionControl/blob/master/mc_mitm/source/controllers/dualshock4_controller.hpp), [DualSense](https://github.com/ndeadly/MissionControl/blob/master/mc_mitm/source/controllers/dualsense_controller.hpp) y [Xbox One](https://github.com/ndeadly/MissionControl/blob/master/mc_mitm/source/controllers/xbox_one_controller.hpp). Esta lista es deliberadamente pequeña. No acepta cualquier dispositivo llamado «Wireless Controller», ni añade perfiles de altavoces, auriculares o dispositivos desconocidos.

El sincronizador importa un emparejamiento válido que ya exista en el sistema origen. No crea uno ni añade a OFW los controladores de MissionControl. Para mandos no nativos, el origen puede necesitar haberse emparejado previamente mediante CFW/MissionControl; en emuMMC también debe funcionar MissionControl. [Requisitos y limitaciones oficiales](https://github.com/ndeadly/MissionControl#supported-controllers). Los modelos/firmwares Xbox que usan BLE y los Xbox 360/otros dispositivos no Bluetooth quedan fuera de este perfil.

Las pruebas usan registros sintéticos para comprobar selección, exclusión y fusión sin alterar la política del lector. No hay una prueba física de cada modelo. Se prioriza Pro Controller; los candidatos ampliados no se presentan como dispositivos validados por el autor.

## Hekate, Atmosphère y consola

- Se conserva Hekate 6.5.4 exacto, identificado por SHA-256.
- Se reconoce también el release oficial exacto Hekate 6.5.5 como **perfil candidato**, sin aceptar binarios modificados ni versiones futuras arbitrarias.
- Una reparación puede conservar la continuación oficial 6.5.4 existente aunque `bootloader/update.bin` sea 6.5.5. Se guardan y comprueban por separado ambos hashes. La instalación nueva usa el release actual reconocido para su continuación.
- El lector de sysMMC sigue limitado al perfil analizado 23.0.1 y al formato de settings `0x220000`. No se añaden formatos nuevos por su parecido con el anterior.
- Cada SD obtiene sus límites FAT32 y hashes de configuración propios. El hardware Erista/Mariko sigue dependiendo del BDK incluido y del firmware admitido; este cambio no constituye validación física de todos los modelos de Switch.
- Atmosphère debe soportar la base Bluetooth externa y tenerla creada. El número de versión de Atmosphère por sí solo no demuestra la compatibilidad del formato de sysMMC o del mando.

## Reparar una configuración cambiada

El payload está vinculado al contenido exacto de `system_settings.ini`, `emummc.ini` y el menú de Hekate. Un cambio legítimo en esas configuraciones puede detener el sincronizador. Se mantiene esta protección y se recompila el sincronizador con los hashes actuales.

En el asistente elige **4: Actualizar/reparar el único sincronizador Bluetooth (con respaldo)**. La opción:

1. Rechaza una transacción pendiente o un historial corrupto. Si cambió el perfil emuMMC del historial, exige una revisión separada; no lo reinicia.
2. Verifica datos y claves localmente, conserva el historial y la base y prepara la versión actualizada con el perfil elegido.
3. Comprueba pruebas publicadas, compilación y auditoría. Muestra el plan antes de pedir `INSTALAR`.
4. Guarda un respaldo privado fuera de la SD y actualiza `BT-Sync-Una-Vez.bin`. Las copias `BT-Sync-Compat…bin` anteriores también se respaldan y se retiran del menú. La continuación permanece sin cambios.

Después abre una sola vez **Hekate → Payloads → BT-Sync-Una-Vez.bin** y entra normalmente a Hack. Si no hay cambios y todas las comprobaciones pasan, el resultado debe ser OK. No hace falta conectar un mando nuevo para comprobar ese camino. Se repite solo cuando cambian o se añaden emparejamientos en el origen; no se ejecuta en cada arranque.

En caso de FALLO, la versión nueva muestra la etapa: 0 preparación, 1 configuración/recuperación, 2 base/historial, 3 lectura sysMMC, 4 lectura/formato de mandos, 5 comparación, 6 escritura/verificación. El número orienta el diagnóstico; no justifica borrar datos o desactivar las comprobaciones.
