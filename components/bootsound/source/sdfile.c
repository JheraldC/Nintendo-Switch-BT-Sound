/*
 * sdfile.c  -  carga de startup.wav en buffer PCM alineado (FASE 2)
 * Portable: stdio + malloc.h + sys/stat.  Sin <switch.h>.
 */
#include "sdfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>

jbs_status_t jbs_load_startup_pcm(const char *path,
                                  jbs_wav_fmt_t *out_fmt,
                                  void **out_buf,
                                  size_t *out_alloc_size,
                                  uint64_t *out_pcm_size)
{
    struct stat st;
    FILE *f;
    uint64_t file_size;
    uint64_t data_off = 0;
    uint64_t data_size = 0;
    uint64_t alloc_size;
    void *buf;
    jbs_status_t rc;

    if (out_fmt)        memset(out_fmt, 0, sizeof(*out_fmt));
    if (out_buf)        *out_buf = NULL;
    if (out_alloc_size) *out_alloc_size = 0;
    if (out_pcm_size)   *out_pcm_size = 0;

    /* --- existencia + tamano (no confiamos en el RIFF para el tamano) --- */
    if (stat(path, &st) != 0)
        return JBS_FILE_NOT_FOUND;
    if (!S_ISREG(st.st_mode))
        return JBS_FILE_NOT_FOUND;
    if (st.st_size < 12)
        return JBS_INVALID_RIFF;
    file_size = (uint64_t)st.st_size;

    /* Descarte temprano: un WAV valido para nosotros es cabecera + PCM (<= MAX) +
     * algun chunk suelto. Un archivo enorme no puede contener PCM aceptable. */
    if (file_size > (uint64_t)JBS_MAX_PCM_SIZE + 0x10000ull)
        return JBS_PCM_TOO_LARGE;

    f = fopen(path, "rb");
    if (!f)
        return JBS_FILE_NOT_FOUND;

    /* --- parseo/validacion RIFF/WAVE --- */
    rc = jbs_wav_probe(f, file_size, out_fmt, &data_off, &data_size);
    if (rc != JBS_OK) {
        fclose(f);
        return rc;
    }

    /* jbs_wav_probe ya garantiza: data_size > 0, multiplo de 4, <= JBS_MAX_PCM_SIZE,
     * y data_off + data_size <= file_size. Revalidamos por defensa en profundidad. */
    if (data_size == 0 || data_size > (uint64_t)JBS_MAX_PCM_SIZE) {
        fclose(f);
        return JBS_PCM_TOO_LARGE;
    }
    if (data_off > file_size || data_size > file_size - data_off) {
        fclose(f);
        return JBS_DATA_NOT_FOUND;
    }

    /* --- reserva alineada: pcm_size real, allocation_size = ALIGN_UP(.,0x1000) --- */
    alloc_size = JBS_ALIGN_UP(data_size, JBS_PCM_ALIGN);
    buf = memalign(JBS_PCM_ALIGN, (size_t)alloc_size);
    if (!buf) {
        fclose(f);
        return JBS_MEMORY_ALLOCATION_FAILED;
    }
    memset(buf, 0, (size_t)alloc_size);   /* cola (allocation_size - pcm_size) a cero */

    /* --- copiar EXACTAMENTE data_size bytes de PCM (sin la cabecera WAV) --- */
    if (fseek(f, (long)data_off, SEEK_SET) != 0 ||
        fread(buf, 1, (size_t)data_size, f) != (size_t)data_size) {
        free(buf);
        fclose(f);
        return JBS_READ_FAILED;
    }

    fclose(f);

    if (out_buf)        *out_buf = buf;
    if (out_alloc_size) *out_alloc_size = (size_t)alloc_size;
    if (out_pcm_size)   *out_pcm_size = data_size;
    return JBS_OK;
}
