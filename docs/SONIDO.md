# BootSound: sonido al entrar a emuMMC

## Instalación sencilla

Para usar el asistente en Windows necesitas [Python 3.11 o posterior](https://www.python.org/downloads/windows/). Extrae por completo el ZIP antes de abrirlo.

Ejecuta `instalar.cmd`, elige **1**, indica la letra de tu SD y selecciona Yoshi o el ejemplo generado. Revisa los archivos y escribe `INSTALAR`. El asistente agrega el módulo y guarda una copia de cualquier archivo reemplazado fuera de la SD. Una selección de sonido que ya exista se conserva.

En una instalación nueva se copian:

```text
atmosphere/contents/42000000004A4253/exefs.nsp
atmosphere/contents/42000000004A4253/flags/boot2.flag
config/JheraldBootSound/startup.wav
config/JheraldBootSound/yoshi_sound.wav  (o startup_example.wav)
config/JheraldBootSound/config.ini
```

El menú sysMMC/emuMMC y `package3` se conservan. El sonido se reproduce al iniciar Atmosphère, cuando el servicio de audio esté disponible, y el módulo termina. No funciona en el sysMMC sin CFW.

BootSound es el nuevo nombre del módulo. Se conserva la carpeta histórica `config/JheraldBootSound/` y el identificador `42000000004A4253` para reutilizar las configuraciones y sonidos existentes. No hace falta mover ni renombrar tus archivos de audio.

## Elegir otro sonido

Guarda tu WAV dentro de `config/JheraldBootSound/` y selecciona su nombre en `config.ini`:

```ini
audio_file=mi_sonido.wav
```

Solo se acepta un nombre de archivo, sin carpetas. Formato: **WAV PCM, estéreo, 48000 Hz, 16 bits**, máximo **2 MiB de datos PCM** (aproximadamente 10,9 segundos). Si la selección falla, el módulo intenta `startup.wav`; si tampoco es válido, termina sin reproducir sonido. El volumen depende del sistema; `volume`, `enabled` y `timeout_ms` no son opciones implementadas.

Para cambiar explícitamente una configuración ya existente desde PC:

```powershell
python tools/install_sd.py audio --sd E:/ --clip yoshi --replace-config
python tools/install_sd.py audio --sd E:/ --clip yoshi --replace-config --apply --backup-dir C:/SwitchBackups/sonido-01
```

La primera instrucción solo muestra el plan. Sustituye `E:` y el respaldo por tus rutas. No uses una carpeta de respaldo que ya exista.

## Compilar las fuentes

Instala devkitPro, **devkitA64**, **libnx** y **switch-tools**. Abre la terminal MSYS2 de devkitPro y ejecuta dentro de `components/bootsound`:

```sh
export DEVKITPRO=/c/devkitPro
make
```

En Linux el valor habitual es `/opt/devkitpro`. El resultado `BootSound.nsp` se instala como `exefs.nsp`. El descriptor es `BootSound.json`.

El código conserva MIT. El clip Yoshi tiene [condiciones separadas](../LICENSES.md); el ejemplo es generado por `scripts/generate_example_startup.py`.
