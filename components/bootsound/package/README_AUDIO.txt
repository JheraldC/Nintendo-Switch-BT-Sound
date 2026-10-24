JheraldBootSound  -  archivo de sonido de arranque
==================================================

RUTA EN LA SD
-------------
    SD:/config/JheraldBootSound/startup.wav
  (para el sysmodule:  sdmc:/config/JheraldBootSound/startup.wav)
  (la letra de unidad que tu sistema asigne a la tarjeta, p.ej. E:\ o F:\ en Windows,
   no tiene ninguna relevancia especial)

El sonido es EXTERNO al binario: para cambiarlo basta sustituir startup.wav,
no hace falta recompilar JheraldBootSound.

DESDE FASE 4.0: puedes guardar VARIOS WAV a la vez dentro de
config/JheraldBootSound/ (p.ej. startup.wav + otro_sonido.wav) y elegir cual
se reproduce con  config/JheraldBootSound/config.ini  (clave audio_file). Si
no hay config.ini, o la clave falta, o el nombre no es seguro, o el archivo
indicado no carga: se usa startup.wav. Ver package/config.ini.example y
docs/SONIDO.md en la raíz de la colección.


FORMATO EXIGIDO  (desde FASE 2, sin conversiones)
--------------------------------------------------
    Contenedor      : RIFF / WAVE
    Codificacion    : PCM lineal sin comprimir  (audio_format = 1)
    Frecuencia      : 48000 Hz   (exacto)
    Bits por muestra: 16
    Canales         : 2 (estereo)
    Orden de bytes  : little-endian
    block_align     : 4 bytes    (2 canales x 16 bits / 8)
    byte_rate       : 192000 bytes/s
    Duracion util   : ~1-3 s recomendado.  Limite duro (desde FASE 4.0): 2 MiB de
                      PCM (~10.9 s). Antes de FASE 4.0 el limite era 1 MiB (~5.46 s).

NO se admite todavia: MP3, AAC, FLAC, OGG, ADPCM, IEEE float, 24/32 bit, mono,
44100 Hz, ni resampling. Si el formato no coincide EXACTAMENTE, el sysmodule
cierra el archivo, libera memoria y termina sin afectar al arranque.

Ejemplo de conversion con ffmpeg:
    ffmpeg -i entrada.cualquiera -ac 2 -ar 48000 -c:a pcm_s16le -f wav startup.wav

Â¿No tienes todavia un sonido en este formato? package/startup_example.wav incluido en este
mismo repositorio ya cumple el formato exacto -- copialo como startup.wav y listo. Es un
arpegio corto (4 notas, ~1 segundo) generado enteramente por codigo
(scripts/generate_example_startup.py), sin ninguna grabacion ni fuente externa.


DIAGNOSTICO OPCIONAL  (solo desarrollo)
--------------------------------------
Un sysmodule boot2 no tiene consola. Para comprobar que el WAV elegido se encontro,
se valido, se cargo y se reprodujo, se usa un ARCHIVO MARCADOR:

    Crear (contenido irrelevante, puede estar vacio):
        SD:/config/JheraldBootSound/debug.flag

    -> En el siguiente arranque el sysmodule escribira (desde FASE 4.0):
        SD:/config/JheraldBootSound/phase4_result.txt

    Borrar debug.flag  -> deja de escribir phase4_result.txt.

Sin debug.flag NO se escribe ningun archivo de diagnostico en ningun arranque.

Nota: phase2_result.txt (FASE 2) y phase3_result.txt (FASE 3.x) son formatos
antiguos de fases anteriores; ya no se generan desde FASE 4.0, pero si existen
en la SD de una instalacion previa no se borran ni se tocan.

Contenido tipico de phase4_result.txt (ejemplo con audio_file por defecto):
    JheraldBootSound FASE 4
    version=0.4.0-fase4.0
    status=OK
    configured_audio_file=(none)
    selected_audio_file=startup.wav
    config_status=NOT_FOUND
    fallback_used=no
    audio_format=1
    sample_rate=48000
    channels=2
    bits_per_sample=16
    block_align=4
    byte_rate=192000
    pcm_bytes=384000
    alloc_bytes=385024
    estimated_duration_ms=2000
    format=PCM
    ...(pasos de audout: service_query, audout_init, start, append,
        released_count, wait, stop -- cada uno OK / FAILED / NOT_RUN)...
    playback=OK

Valores posibles de status:
    OK
    SD_MOUNT_FAILED
    FILE_NOT_FOUND
    READ_FAILED
    INVALID_RIFF
    INVALID_WAVE
    FMT_NOT_FOUND
    DATA_NOT_FOUND
    UNSUPPORTED_FORMAT
    INVALID_BLOCK_ALIGN
    INVALID_BYTE_RATE
    PCM_TOO_LARGE
    MEMORY_ALLOCATION_FAILED

Valores posibles de config_status:
    NOT_FOUND           (no existe config.ini -> se usa startup.wav)
    OK                  (config.ini leido correctamente)
    INVALID_AUDIO_FILE  (audio_file presente pero invalido -> se usa startup.wav)


IMPORTANTE
----------
Desde FASE 3.3 el sonido SI se reproduce de verdad (audout), una sola vez, sin
dejar hilos ni servicios abiertos. Desde FASE 4.0 el archivo a reproducir es
seleccionable mediante config.ini (clave audio_file), con reserva automatica a
startup.wav si algo falla. El binario mismo no cambia entre instalaciones: solo
cambian los .wav y el config.ini dentro de config/JheraldBootSound/.
