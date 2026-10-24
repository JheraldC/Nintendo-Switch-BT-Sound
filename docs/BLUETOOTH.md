# BT Sync Una Vez

Importa selectivamente mandos Pro/Joy-Con compatibles de sysMMC a emuMMC dentro de una misma Switch. Conserva los mandos exclusivos de emuMMC. Si un mando está en ambos sistemas puede actualizar su clave en la base de emuMMC con la de sysMMC. No importa dispositivos de audio ni nombres no reconocidos. Si no cabe la fusión o el perfil no es conocido, falla sin completar una nueva sincronización.

## Requisitos del destino

- sysMMC 23.0.1. Es el único perfil analizado; no basta con tener el último Atmosphère.
- SD FAT32, emuMMC habilitada y entradas de arranque válidas para sysMMC y emuMMC.
- `bootloader/update.bin` oficial de Hekate 6.5.4, SHA-256 `95f6e933e32e028f3ae90f0b0857ff47800e834bdace38a0ab019d8ed35a8c24`.
- Atmosphère con soporte de base externa, configurado como sigue. La referencia utilizada es [Atmosphère 1.12.1](https://github.com/Atmosphere-NX/Atmosphere/blob/1.12.1/config_templates/system_settings.ini).
- `switch/prod.keys` propio de esa consola, con `bis_key_02` y `save_mac_key` válidas. Se leen localmente, nunca se suben al repositorio.
- Windows, Python 3.11+, devkitARM y WSL con `gcc`, `make` y `python3` para compilar y ejecutar las comprobaciones.

Antes de activar la base externa, respalda tu configuración y los emparejamientos existentes. En `atmosphere/config/system_settings.ini`, conserva las opciones existentes y activa dentro de `[atmosphere]`:

```ini
[atmosphere]
enable_external_bluetooth_db = u8!0x1
```

Arranca emuMMC una vez para que Atmosphère cree `atmosphere/bluetooth_devices.db` a partir de sus emparejamientos actuales. Apaga la consola y vuelve a conectar la SD al PC. El asistente comprueba esta precondición y no activa esta opción automáticamente.

## Instalar para una consola

Extrae una copia nueva del proyecto por SD. Configura devkitARM si no está en la ruta:

```powershell
$env:DEVKITARM = 'C:/devkitPro/devkitARM'
```

Abre `instalar.cmd` y elige 3. El asistente detecta los nombres de las entradas sysMMC y emuMMC por su configuración. Confirma esos nombres y el firmware. El asistente prepara, compila, prueba y audita antes de mostrar el plan. Solo instala tras escribir `INSTALAR`; antes guarda y verifica un respaldo privado en el PC. El menú y la base existente se mantienen.

Para hacer lo mismo manualmente, desde `components/bluetooth-sync`:

```powershell
python prepare.py --sd E:/ --sysmmc 'sysMMC' --emummc 'emuMMC' --firmware-profile 23.0.1
python build_payload.py
python tests/test_portable.py
python tests/test_result_paths.py
python run_host_tests.py
python audit_auto.py
python deploy_portable.py --sd E:/
python deploy_portable.py --sd E:/ --apply --backup-dir C:/SwitchBackups/consola-bt-01
```

El instalador añade exactamente cuatro archivos: `bootloader/payloads/BT-Sync-Una-Vez.bin`, continuación verificada de Hekate, historial inicial vacío y LEEME dentro de `switch/bt-autosync-v1/`. No sincroniza durante la instalación. Rechaza una instalación o transacción previa en lugar de sobrescribirla.

En los comandos, usa los nombres exactos de las entradas de tu Hekate aunque sean distintos de `sysMMC` y `emuMMC`. El asistente y el instalador conservan esos nombres.

El binario se vincula a hashes de configuración y límites de partición de esa SD. Cambiar la configuración puede exigir preparar de nuevo. No hay un `.bin` universal para copiar de una consola a otra. Los nombres internos `bt-autosync-v1` son históricos: esta edición se ejecuta manualmente.

## Usarlo

1. Con respaldo verificado, abre Hekate → Payloads → BT-Sync-Una-Vez.bin.
2. La pantalla muestra el progreso y el resultado: OK en verde o FALLO en rojo. Vuelve automáticamente al menú; si no puede continuar, se apaga. VOL− al inicio cancela la operación.
3. Entra a emuMMC y comprueba la conexión de los controles sin volver a emparejarlos.
4. En los siguientes arranques entra directamente a emuMMC. Si emparejas más mandos en sysMMC, puedes ejecutar otra vez el payload.

No retires la SD ni cortes la energía durante la operación. Si se interrumpe una transacción, ejecuta de nuevo el payload para recuperar antes de iniciar emuMMC. Una recuperación puede mostrar FALLO para no presentarse como una importación nueva exitosa. Si persiste, utiliza el respaldo y detente a revisar.

El transporte bloquea escrituras NAND. El lector autenticado solo extrae los datos Bluetooth admitidos y las escrituras se limitan a la transacción en SD con NAND desconectada. Las pruebas sintéticas emplean AES/CMAC simulados; no prueban nuevas consolas ni todos los fallos físicos de una SD.

`migrate_original.py` solo convierte la instalación automática legacy identificada por hashes exactos. Es una herramienta avanzada; no la ejecutes sobre una instalación distinta. `rollback.py` requiere el respaldo privado; consulta su ayuda.

Fuentes y licencias GPL-2.0 incluidas. [Hekate oficial](https://github.com/CTCaer/hekate/releases/tag/v6.5.4).
