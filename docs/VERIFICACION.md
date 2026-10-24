# Verificación de las publicaciones

## BootSound 0.4.1 — colección 1.0.3

El módulo se recompiló con devkitA64/libnx para el cambio de nombre a BootSound. Se verificaron el contenedor PFS0, el ejecutable NSO, el nombre `BootSound` en el NPDM y el identificador `42000000004A4253`. El descriptor conserva todos sus campos y permisos excepto el nombre.

La versión conserva la lógica de audio y la carpeta histórica de configuración. Se repiten las comprobaciones de descriptor, WAV, configuración, lógica de audio e IPC, junto con las pruebas del instalador. El nuevo nombre también se generó en la imagen a partir de su código.

El binario actual y sus datos están en [BootSound/release/BUILD.json](../components/bootsound/release/BUILD.json). Esta edición está comprobada en PC y pendiente de su prueba física específica. No se instaló en la SD real durante la publicación.

## Registro histórico de la publicación 1.0.0

Comprobaciones ejecutadas en PC el 23 de octubre de 2026. Todas pasaron:

- **8 pruebas** del instalador de sonido/logo y de las herramientas de imagen: revisión sin escribir, respaldo, conservación de configuración, rechazo de cambios inesperados y límites/formato de píxeles.
- Compilación de JheraldBootSound con devkitA64/libnx. El binario histórico publicado de **90466 bytes**, SHA-256 `212cbf0cfb97f5beafd84125aff3c444b8332868db1503859d03f4f225d2b82a`, coincidía con la SD del autor. La recompilación produjo **90458 bytes** y otro hash; no se afirma reproducción binaria exacta. El generador NPDM avisó sobre cuatro campos opcionales ausentes y terminó correctamente. En esa publicación se conservó el binario histórico para instalar.
- Sonido: **29 fixtures WAV** aceptados/rechazados según lo esperado, pruebas de configuración, lógica de espera/audio y comprobación del IPC TIPC. Parte de estas pruebas usa implementaciones de referencia Python; no son una reproducción de audio física.
- Bluetooth: compilación ARM para **bindings sintéticos**, auditoría de símbolos/política NAND/ámbito de escrituras, **8 pruebas** de instalación portátil y **46 comprobaciones** de la interfaz real extraída de `main.c`, con ASan/UBSan. Incluyen el progreso, errores, colores, 500 ms en 100% y 3000 ms de resultado.
- Se volvió a ejecutar la suite completa del lector/fusión/FatFs con medios sintéticos y ASan/UBSan: 26.599.175 comprobaciones de perfil Bluetooth, 90.982 de GPT, 30.183 de fusión/transacciones, 23.845 de la capa SD y 2.258 casos del lector, además de las suites de geometría, autenticación, cabeceras y continuación de Hekate.

Los recuentos completos están en [evidence/validation.json](../components/bluetooth-sync/evidence/validation.json). AES/CMAC se simulan en las pruebas de medios; SHA usa la implementación congelada real. “Capa SD” en estos resultados significa FatFs real sobre un medio ficticio de PC, no escrituras de prueba en la microSD del autor.

La comprobación física disponible es el reporte del autor de **OK** y de la pantalla de progreso. No hay una nueva prueba física documentada del asistente público, de la pausa de 0,5 s, de todas las variantes de hardware o de reconexión de mandos en otras consolas. No se certifica compatibilidad universal.

La publicación usa una lista explícita de archivos registrados en Git para formar el ZIP. Los bindings, salidas de compilación Bluetooth, claves, bases e historiales reales y respaldos se excluyen. Se comprueban la integridad del ZIP y los hashes de cada archivo. El asistente no se aplicó a la SD real durante esta publicación.

## Repetir las pruebas

Desde la raíz:

```powershell
python -m pip install -r requirements.txt
python -m unittest discover -s tests -v
```

Desde `components/bootsound`:

```powershell
python tests/gen_fixtures.py
python tests/run_tests.py
python tests/run_config_tests.py
python tests/run_audio_tests.py
python tests/check_ipc_dispatch.py
```

Para Bluetooth sin consola, desde su componente y con devkitARM/WSL configurados:

```powershell
python tests/test_portable.py --prepare-synthetic
python build_payload.py
python tests/test_portable.py
python tests/test_result_paths.py
python run_host_tests.py
python audit_auto.py
```

Los bindings y el binario de este último ejemplo son sintéticos: **no se instalan**. Para una SD real utiliza una copia limpia y el procedimiento de la guía Bluetooth.
