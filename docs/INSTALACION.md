# Instalador desde un enlace

Los comandos del README descargan el proyecto público desde este mismo GitHub y abren el asistente en tu ordenador. No necesitas un servidor local, contratar hosting ni mantener encendido el ordenador del autor. Sí necesitas Internet para la primera descarga y para las dependencias que todavía no estén en caché.

## Sistemas

- Windows 10/11 de 64 bits, PowerShell y `tar.exe` incluido en Windows.
- macOS 12 o posterior, Intel o Apple Silicon, con `curl`, `tar` y `shasum` del sistema.
- Ubuntu 20.04+ y Fedora actuales, x86-64 o ARM64, con glibc 2.28+, `curl`, `tar`, `sha256sum` y una sesión de terminal. Para Bluetooth, `findmnt`, `lsblk` y sysfs permiten leer la geometría de la partición sin acceder al dispositivo en bruto.

El asistente usa un Python 3.12 portátil fijado y verificado. No instala paquetes con sudo, apt, dnf, Homebrew o winget. El selector usa el diálogo de Windows, `osascript` en macOS, o Zenity/KDialog/Tk si están disponibles en Linux. En un escritorio sin selector o en una sesión sin interfaz gráfica, acepta la ruta del archivo en la terminal.

## Qué descarga

1. El lanzador obtiene el Python portátil verificado (unos 21–34 MiB) y una revisión exacta de este repositorio. Reutiliza las copias verificadas en `~/.bt-sound/`.
2. Para audio propio que requiera conversión, reutiliza FFmpeg instalado o obtiene un ejecutable estático verificado, empaquetado por imageio-ffmpeg. Descarga de unos 22–65 MiB según la plataforma. Solo se extraen FFmpeg y sus avisos: no se instala imageio ni su biblioteca Python. El binario incluye sus códecs; no es una compilación exclusiva de audio.
3. Solo al preparar Bluetooth, reutiliza devkitARM/arm-none-eabi existentes o descarga xPack GNU Arm Embedded GCC (unos 280–320 MiB). Conserva compilador, binutils, cabeceras, bibliotecas y licencias; omite documentación y herramientas no usadas. La compilación y auditoría son automáticas y locales.

Las versiones, URL, tamaños y SHA-256 de las dependencias están en `tools/runtime-manifest.json`. Las fuentes y licencias se enlazan en `LICENSES.md`. No se guarda un ejecutable grande de FFmpeg en el repositorio ni se instala un servicio en segundo plano. `BT_SOUND_CACHE` permite elegir otra carpeta de caché.

## Uso

1. Conecta la SD ya configurada. En Windows usa su raíz, como `E:/`; en macOS, `/Volumes/SWITCH SD`; en Linux, el punto de montaje exacto, como `/media/usuario/SWITCH SD`.
2. Abre el asistente con el comando del README y elige sonido, logo de Hekate o Bluetooth.
3. El asistente prepara y muestra los archivos que cambiarán. Revisa el destino y confirma `INSTALAR` para crear el respaldo y aplicar. Cancelar antes de ese paso no escribe en la SD.
4. Expulsa la SD de forma segura y prueba en la consola. La conversión o compilación correcta no sustituye la prueba física.

Los audios propios, claves y emparejamientos se procesan en tu ordenador. La función de descarga solo solicita dependencias públicas; no envía los archivos de la SD ni sus rutas a un servicio de conversión.

## Bluetooth

La opción automática conserva los requisitos de `docs/BLUETOOTH.md`: sysMMC 23.0.1, Hekate 6.5.4 exacto, partición FAT32, emuMMC y base Bluetooth externa creadas y claves propias válidas. No actualiza firmware, no obtiene claves ni modifica particiones. Una instalación o transacción Bluetooth previa se rechaza para no sobrescribir su estado.

Las pruebas C con ASan/UBSan se ejecutan en el desarrollo/CI sobre medios sintéticos. La distribución contiene evidencia ligada a los hashes de esas fuentes. En el ordenador del usuario se vuelve a verificar esa evidencia, se generan los enlaces a su SD, se compila el payload y se audita el binario y sus dependencias. Los enlaces generados se comprueban contra la preparación y la compilación; no se reutilizan los de otra consola. No hace falta instalar un compilador C de escritorio, WSL ni make para este flujo.

No se afirma compatibilidad con versiones futuras de Hekate ni con otros perfiles de sysMMC. En particular, ampliar el perfil a Hekate 6.5.5 queda pendiente de su prueba física; el número de versión del instalador no constituye esa validación.

## Uso local y conversión sin SD

Si prefieres descargar las fuentes, puedes ejecutar `instalar.cmd`, `sh instalar.sh` o `python3 instalar.py`. Ese método requiere Python ya disponible. No hace falta instalar los requisitos de desarrollo para el asistente.

```sh
python3 tools/audio_convert.py mi_audio.mp3 mi_sonido.wav --start 1 --duration 8 --download-ffmpeg
```

Este comando crea un archivo nuevo, no sobrescribe el original y no escribe en una SD. En Windows usa `python` si ese es el nombre de tu instalación. El código de BootSound continúa siendo el mismo; solo cambia el recurso WAV.
