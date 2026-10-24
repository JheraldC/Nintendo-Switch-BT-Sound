# Verificación de la versión 1.0.0

Reuní el sonido de arranque, las imágenes y la sincronización de controles en una sola versión del proyecto: **1.0.0**.

## Sonido de arranque

Compilé BootSound con devkitA64/libnx. Verifiqué el contenedor PFS0, el ejecutable NSO, el nombre BootSound en el NPDM y el identificador `42000000004A4253`. Conservé la carpeta de configuración y los permisos del módulo.

Incluyo el tamaño, el hash y los datos de compilación en [BUILD.json](../components/bootsound/release/BUILD.json).

## Sincronización de controles

Compilé el payload ARM con una configuración ficticia para comprobarlo en PC. Revisé la política de lectura de sysMMC, los límites de escritura en la SD y el regreso al menú de Hekate. Las pruebas cubren la lectura del perfil Bluetooth, la fusión de mandos, las transacciones, su recuperación, FatFs y la interfaz de progreso y resultados.

Para las pruebas de medios uso AES/CMAC simulados y la implementación SHA congelada. En la capa SD uso FatFs real sobre medios ficticios. Registro estos resultados en [validation.json](../components/bluetooth-sync/evidence/validation.json).

## Instalación y descarga

Compruebo que el instalador conserve las configuraciones existentes, detecte los nombres de sysMMC y emuMMC y rechace selecciones ambiguas. Genero el ZIP desde los archivos registrados en Git y verifico su integridad y el hash de cada archivo.

## Repetir las comprobaciones

Desde la raíz:

```powershell
python -m pip install -r requirements.txt
python -m unittest discover -s tests -v
```

Desde `components/bootsound`:

```powershell
python scripts/verify_json.py
python tests/gen_fixtures.py
python tests/run_tests.py
python tests/run_config_tests.py
python tests/run_audio_tests.py
python tests/check_ipc_dispatch.py
```

Desde `components/bluetooth-sync`, con devkitARM y WSL configurados:

```powershell
python tests/test_portable.py --prepare-synthetic
python build_payload.py
python tests/test_portable.py
python tests/test_result_paths.py
python run_host_tests.py
python audit_auto.py
```

La configuración de este último ejemplo es ficticia y sirve para las comprobaciones de PC. Para instalar en una consola, utiliza su propia SD y el procedimiento de la [guía Bluetooth](BLUETOOTH.md).
