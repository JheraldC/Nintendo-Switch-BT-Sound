# BootSound: sonido al entrar a emuMMC

## Instalación sencilla

Puedes iniciar el asistente en Windows, macOS o Linux con los [comandos de un enlace](INSTALACION.md). Ese método prepara Python automáticamente. Para usar una copia local de las fuentes necesitas Python 3.11 o posterior; ejecuta `python instalar.py` (en macOS/Linux, `python3 instalar.py`).

Elige 1, indica la raíz de tu SD y selecciona Yoshi, el ejemplo generado o **3: elegir mi archivo**. En la tercera opción se abre un selector nativo; si el escritorio no lo ofrece, puedes pegar la ruta. Cancelarlo no instala nada. Indica el segundo de inicio y una duración de hasta 10 segundos. El asistente convierte localmente el audio, comprueba el WAV resultante y muestra el plan antes de pedir `INSTALAR`.

MP3, M4A, AAC, FLAC, OGG, WAV y otros formatos admitidos por el FFmpeg utilizado pueden convertirse; también se puede extraer la primera pista de audio de un vídeo local. El original se conserva. El resultado se instala como `config/JheraldBootSound/custom_sound.wav` y se selecciona en `config.ini`, conservando sus comentarios y otras opciones. Un sonido propio anterior con ese nombre y la configuración se respaldan antes del reemplazo. Para los sonidos incluidos, el asistente permite conservar o cambiar explícitamente la selección actual.

Se reutiliza FFmpeg de `FFMPEG_BINARY` o PATH. Si falta, se descarga la distribución portátil fijada en `tools/runtime-manifest.json`, se verifica su SHA-256 y se conserva solo su ejecutable y sus avisos de licencia. No se necesitan ffprobe, ffplay, DLL separadas, imageio, NumPy ni librerías Python de audio. Un WAV corto que ya cumple el formato no requiere FFmpeg ni descarga. FFmpeg convierte audio; no descompila ni adapta el ejecutable del módulo a otra versión de Atmosphère.

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

Solo se acepta un nombre de archivo, sin carpetas. Formato: WAV PCM, estéreo, 48000 Hz, 16 bits, máximo 2 MiB de datos PCM (aproximadamente 10,9 segundos). Si la selección falla, el módulo intenta `startup.wav`; si tampoco es válido, termina sin reproducir sonido. El volumen depende del sistema; `volume`, `enabled` y `timeout_ms` no son opciones implementadas.

Para cambiar explícitamente una configuración ya existente desde PC:

```powershell
python tools/install_sd.py audio --sd E:/ --clip yoshi --replace-config
python tools/install_sd.py audio --sd E:/ --clip yoshi --replace-config --apply --backup-dir C:/SwitchBackups/sonido-01
```

La primera instrucción solo muestra el plan. Sustituye `E:` y el respaldo por tus rutas. No uses una carpeta de respaldo que ya exista.

## Compilar las fuentes

Esta sección es para desarrollar el módulo: no se necesita compilar BootSound para convertir o instalar un sonido.

Instala devkitPro, devkitA64, libnx y switch-tools. Abre la terminal MSYS2 de devkitPro y ejecuta dentro de `components/bootsound`:

```sh
export DEVKITPRO=/c/devkitPro
make
```

En Linux el valor habitual es `/opt/devkitpro`. El resultado `BootSound.nsp` se instala como `exefs.nsp`. El descriptor es `BootSound.json`.

El código conserva MIT. El clip Yoshi tiene [condiciones separadas](../LICENSES.md); el ejemplo es generado por `scripts/generate_example_startup.py`.
