/*
 * wav.h  -  parser RIFF/WAVE minimo y defensivo para JheraldBootSound (FASE 2)
 *
 * NO depende de <switch.h>: opera sobre un FILE* + tamano declarado, de modo que
 * el MISMO codigo se compila y se prueba en PC (ver tests/).
 *
 * Formato admitido en FASE 2 (y solo ese):
 *   RIFF/WAVE, PCM lineal (audio_format = 1), 48000 Hz, 16 bits, 2 canales, little-endian.
 */
#ifndef JBS_WAV_H
#define JBS_WAV_H

#include <stdint.h>
#include <stdio.h>

/* ---- formato exacto exigido ---- */
#define JBS_WAV_FORMAT_PCM        1u
#define JBS_WAV_SAMPLE_RATE       48000u
#define JBS_WAV_CHANNELS          2u
#define JBS_WAV_BITS_PER_SAMPLE   16u
#define JBS_WAV_BLOCK_ALIGN       4u        /* channels * bits/8 = 2 * 2            */
#define JBS_WAV_BYTE_RATE         192000u   /* sample_rate * block_align            */

/* ---- limites defensivos ----
 * FASE 4.0 sube el limite de 1 MiB a 2 MiB para admitir audios "enhanced" mas
 * largos que el objetivo original de ~1-3 s (validado en hardware con FASE 3.3:
 * 6,592 s / 1.265.664 B). A 48k/16/stereo: 1 s = 192000 B, 2 MiB = ~10,92 s.
 * Si algun dia se necesita un sonido aun mas largo hay que subir ESTE valor y,
 * a la vez, INNER_HEAP_SIZE en source/main.c (van acoplados).
 */
#define JBS_MAX_PCM_SIZE          (2u * 1024u * 1024u)   /* 2 MiB */

/* alineacion del buffer PCM final (pensando ya en AudioOutBuffer de FASE 3) */
#define JBS_PCM_ALIGN             0x1000u

#define JBS_ALIGN_UP(n, a)        ( ((uint64_t)(n) + ((uint64_t)(a) - 1u)) & ~((uint64_t)(a) - 1u) )

/* ---- estados (compartidos por todo el sysmodule: parser, filesystem, audio,
 * debug.c / phase2_result.txt / phase3_result.txt). Se definen aqui porque
 * wav.h no depende de nada mas, no porque audio.c/sdfile.c sean parte del
 * parser. Los valores existentes NUNCA se renumeran; lo nuevo se anade al final. */
typedef enum {
    JBS_OK = 0,
    JBS_SD_MOUNT_FAILED,
    JBS_FILE_NOT_FOUND,
    JBS_READ_FAILED,
    JBS_INVALID_RIFF,
    JBS_INVALID_WAVE,
    JBS_FMT_NOT_FOUND,
    JBS_DATA_NOT_FOUND,
    JBS_UNSUPPORTED_FORMAT,
    JBS_INVALID_BLOCK_ALIGN,
    JBS_INVALID_BYTE_RATE,
    JBS_PCM_TOO_LARGE,
    JBS_MEMORY_ALLOCATION_FAILED,
    /* ---- FASE 3 (audout) ---- */
    JBS_AUDIO_SERVICE_TIMEOUT,      /* audout:u no aparecio dentro del plazo */
    JBS_AUDIO_INIT_FAILED,          /* audoutInitialize() fallo (no timeout) */
    JBS_AUDIO_DEVICE_MISMATCH,      /* sample_rate/channels/formato del dispositivo != esperado */
    JBS_AUDIO_START_FAILED,         /* audoutStartAudioOut() fallo */
    JBS_AUDIO_APPEND_FAILED,        /* audoutAppendAudioOutBuffer() fallo */
    JBS_PLAYBACK_TIMEOUT,           /* audoutWaitPlayFinish() agoto su timeout */
    JBS_PLAYBACK_FAILED,            /* audoutWaitPlayFinish() fallo por otro motivo */
    JBS_AUDIO_STOP_FAILED,          /* audoutStopAudioOut() fallo (no crítico, se reporta igual) */
    /* ---- FASE 3.1 (AtmosphereHasService en vez de hilo trabajador) ---- */
    JBS_AUDIO_SERVICE_QUERY_FAILED  /* la propia consulta AtmosphereHasService fallo (no es timeout) */
} jbs_status_t;

const char *jbs_status_str(jbs_status_t s);

/* ---- valores leidos del chunk "fmt " (informativo / diagnostico) ---- */
typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} jbs_wav_fmt_t;

/*
 * jbs_wav_probe: valida cabecera RIFF/WAVE y localiza el PCM.
 *
 *   f          : FILE* abierto en modo binario (se hace fseek internamente).
 *   file_size  : tamano real del archivo (obtenido con stat, NO se confia en el
 *                campo de tamano del RIFF).
 *   out_fmt    : [salida] siempre se rellena a 0 y luego con lo leido de "fmt "
 *                (util para el diagnostico aunque el formato sea invalido).
 *   out_data_offset / out_data_size : [salida] posicion y longitud del PCM dentro
 *                del archivo. Solo validos si el retorno es JBS_OK.
 *
 * Devuelve JBS_OK o el estado de error correspondiente. Nunca aborta.
 * Trata el archivo como entrada NO confiable: comprueba truncados, tamanos de
 * chunk fuera de limites, cabeceras incompletas, padding impar y overflow de
 * offset+size.
 *
 * FASE 2.1: el campo riff_size (bytes 4..7) SI se usa. riff_end = 8 + riff_size
 * es el limite real del contenedor RIFF; el recorrido de chunks queda acotado
 * estrictamente a [12, riff_end), nunca a [12, file_size). Si riff_end > file_size
 * (el RIFF declara mas de lo que existe) -> JBS_INVALID_RIFF. Si file_size > riff_end,
 * los bytes sobrantes son "trailing data" y se ignoran sin recorrerlos.
 */
jbs_status_t jbs_wav_probe(FILE *f, uint64_t file_size,
                           jbs_wav_fmt_t *out_fmt,
                           uint64_t *out_data_offset,
                           uint64_t *out_data_size);

#endif /* JBS_WAV_H */
