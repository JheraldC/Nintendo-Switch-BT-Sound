/*
 * wav.c  -  parser RIFF/WAVE minimo y defensivo (FASE 2)
 * Portable: solo <stdint.h>/<stdio.h>/<string.h>. Sin <switch.h>. Se prueba en PC.
 */
#include "wav.h"
#include <string.h>

const char *jbs_status_str(jbs_status_t s)
{
    switch (s) {
        case JBS_OK:                       return "OK";
        case JBS_SD_MOUNT_FAILED:          return "SD_MOUNT_FAILED";
        case JBS_FILE_NOT_FOUND:           return "FILE_NOT_FOUND";
        case JBS_READ_FAILED:              return "READ_FAILED";
        case JBS_INVALID_RIFF:             return "INVALID_RIFF";
        case JBS_INVALID_WAVE:             return "INVALID_WAVE";
        case JBS_FMT_NOT_FOUND:            return "FMT_NOT_FOUND";
        case JBS_DATA_NOT_FOUND:           return "DATA_NOT_FOUND";
        case JBS_UNSUPPORTED_FORMAT:       return "UNSUPPORTED_FORMAT";
        case JBS_INVALID_BLOCK_ALIGN:      return "INVALID_BLOCK_ALIGN";
        case JBS_INVALID_BYTE_RATE:        return "INVALID_BYTE_RATE";
        case JBS_PCM_TOO_LARGE:            return "PCM_TOO_LARGE";
        case JBS_MEMORY_ALLOCATION_FAILED: return "MEMORY_ALLOCATION_FAILED";
        case JBS_AUDIO_SERVICE_TIMEOUT:    return "AUDIO_SERVICE_TIMEOUT";
        case JBS_AUDIO_INIT_FAILED:        return "AUDIO_INIT_FAILED";
        case JBS_AUDIO_DEVICE_MISMATCH:    return "AUDIO_DEVICE_MISMATCH";
        case JBS_AUDIO_START_FAILED:       return "AUDIO_START_FAILED";
        case JBS_AUDIO_APPEND_FAILED:      return "AUDIO_APPEND_FAILED";
        case JBS_PLAYBACK_TIMEOUT:         return "PLAYBACK_TIMEOUT";
        case JBS_PLAYBACK_FAILED:          return "PLAYBACK_FAILED";
        case JBS_AUDIO_STOP_FAILED:        return "AUDIO_STOP_FAILED";
        case JBS_AUDIO_SERVICE_QUERY_FAILED: return "AUDIO_SERVICE_QUERY_FAILED";
        default:                           return "UNKNOWN";
    }
}

static uint16_t rd_u16le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_u32le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* Lee exactamente n bytes desde el offset absoluto off. 0 = OK, -1 = fallo/truncado. */
static int read_at(FILE *f, uint64_t off, void *dst, size_t n)
{
    if (fseek(f, (long)off, SEEK_SET) != 0)
        return -1;
    if (fread(dst, 1, n, f) != n)
        return -1;
    return 0;
}

jbs_status_t jbs_wav_probe(FILE *f, uint64_t file_size,
                           jbs_wav_fmt_t *out_fmt,
                           uint64_t *out_data_offset,
                           uint64_t *out_data_size)
{
    jbs_wav_fmt_t fmt;
    uint8_t hdr[12];
    uint64_t pos, riff_end;
    uint32_t riff_size;
    int have_fmt = 0;
    int have_data = 0;
    uint64_t data_off = 0;
    uint64_t data_size = 0;

    memset(&fmt, 0, sizeof(fmt));
    if (out_fmt)         *out_fmt = fmt;
    if (out_data_offset) *out_data_offset = 0;
    if (out_data_size)   *out_data_size = 0;

    /* --- cabecera RIFF/WAVE (12 bytes minimo) --- */
    if (file_size < 12)
        return JBS_INVALID_RIFF;
    if (read_at(f, 0, hdr, sizeof(hdr)) != 0)
        return JBS_READ_FAILED;
    if (memcmp(hdr, "RIFF", 4) != 0)
        return JBS_INVALID_RIFF;
    if (memcmp(hdr + 8, "WAVE", 4) != 0)
        return JBS_INVALID_WAVE;

    /* --- riff_size / riff_end: limite REAL y declarado del contenedor RIFF ---
     * hdr[4..7] = riff_size = bytes que siguen a este campo (>= "WAVE" + chunks).
     * riff_end  = 8 (magic "RIFF" + campo size) + riff_size.
     * Aritmetica en uint64_t: riff_size es como mucho 0xFFFFFFFF (u32), 8+eso
     * jamas desborda un uint64_t (evita wraparound si se hiciera en 32 bits). */
    riff_size = rd_u32le(hdr + 4);
    if (riff_size < 4)                       /* debe cubrir al menos "WAVE" */
        return JBS_INVALID_RIFF;
    riff_end = 8u + (uint64_t)riff_size;
    if (riff_end > file_size)
        return JBS_INVALID_RIFF;             /* declara datos que fisicamente no existen */
    /* file_size > riff_end esta permitido (trailing data de otra herramienta/contenedor);
     * esos bytes NO se recorren ni se interpretan como chunks: el bucle queda acotado
     * estrictamente a [12, riff_end), nunca a [12, file_size). */

    /* --- recorrido dinamico de chunks, limitado a [12, riff_end) --- */
    pos = 12;
    while (pos + 8 <= riff_end) {
        uint8_t ch[8];
        uint32_t csize;
        uint64_t body;
        uint64_t adv;

        if (read_at(f, pos, ch, sizeof(ch)) != 0)
            return JBS_READ_FAILED;

        csize = rd_u32le(ch + 4);
        body  = pos + 8;                 /* pos+8 <= riff_end (guarda del while) */

        /* limite generico por chunk: cabecera + cuerpo COMPLETO dentro de riff_end.
         * (pos+8<=riff_end ya implica body<=riff_end; se deja explicito por defensa
         * en profundidad, tal como exige cada uno de: header_end/body_end/padded_end). */
        if (body > riff_end || csize > riff_end - body) {
            if (memcmp(ch, "data", 4) == 0)
                return JBS_DATA_NOT_FOUND;   /* el PCM se saldria de riff_end */
            break;                           /* chunk truncado por riff_end: fin de recorrido util */
        }

        if (memcmp(ch, "fmt ", 4) == 0) {
            uint8_t fb[16];
            /* fmt PCM canonico = 16 bytes. Puede venir mayor (cbSize / extensible);
             * leemos solo los 16 primeros y validamos audio_format.
             * body+16 <= body+csize <= riff_end ya garantizado arriba (csize>=16). */
            if (csize < 16)
                return JBS_UNSUPPORTED_FORMAT;
            if (read_at(f, body, fb, sizeof(fb)) != 0)
                return JBS_READ_FAILED;
            fmt.audio_format    = rd_u16le(fb + 0);
            fmt.num_channels    = rd_u16le(fb + 2);
            fmt.sample_rate     = rd_u32le(fb + 4);
            fmt.byte_rate       = rd_u32le(fb + 8);
            fmt.block_align     = rd_u16le(fb + 12);
            fmt.bits_per_sample = rd_u16le(fb + 14);
            have_fmt = 1;
            if (out_fmt) *out_fmt = fmt;        /* exponer aunque luego falle */
        } else if (memcmp(ch, "data", 4) == 0) {
            /* csize > riff_end-body ya descartado arriba: el PCM cabe entero en riff_end. */
            data_off  = body;
            data_size = csize;
            have_data = 1;
        }
        /* else: chunk desconocido (LIST/JUNK/fact/bext/...) -> se ignora con seguridad */

        if (have_fmt && have_data)
            break;

        /* avanzar: cuerpo + padding RIFF (1 byte si csize es impar), tambien dentro de riff_end */
        adv = (uint64_t)csize + (uint64_t)(csize & 1u);
        if (adv > riff_end - body)          /* el padding se saldria de riff_end */
            break;                          /* fin de recorrido util */
        pos = body + adv;
    }

    /* --- validacion final --- */
    if (!have_fmt)
        return JBS_FMT_NOT_FOUND;

    if (fmt.audio_format != JBS_WAV_FORMAT_PCM)
        return JBS_UNSUPPORTED_FORMAT;
    if (fmt.num_channels != JBS_WAV_CHANNELS ||
        fmt.sample_rate != JBS_WAV_SAMPLE_RATE ||
        fmt.bits_per_sample != JBS_WAV_BITS_PER_SAMPLE)
        return JBS_UNSUPPORTED_FORMAT;
    if (fmt.block_align != JBS_WAV_BLOCK_ALIGN)
        return JBS_INVALID_BLOCK_ALIGN;
    if (fmt.byte_rate != JBS_WAV_BYTE_RATE)
        return JBS_INVALID_BYTE_RATE;

    if (!have_data || data_size == 0)
        return JBS_DATA_NOT_FOUND;
    if ((data_size % JBS_WAV_BLOCK_ALIGN) != 0)   /* debe ser un numero entero de frames */
        return JBS_UNSUPPORTED_FORMAT;
    if (data_size > JBS_MAX_PCM_SIZE)
        return JBS_PCM_TOO_LARGE;

    if (out_data_offset) *out_data_offset = data_off;
    if (out_data_size)   *out_data_size = data_size;
    return JBS_OK;
}
