/*
 * sdfile.h  -  abrir/validar/cargar startup.wav en un buffer PCM alineado (FASE 2)
 *
 * Portable (stdio + malloc.h + sys/stat): mismo codigo en PC y en Switch.
 * En Switch el prefijo "sdmc:/" lo resuelve el devoptab de libnx (fsdevMountSdmc).
 */
#ifndef JBS_SDFILE_H
#define JBS_SDFILE_H

#include <stddef.h>
#include <stdint.h>
#include "wav.h"

/*
 * jbs_load_startup_pcm:
 *   path            : ruta al WAV (p.ej. "sdmc:/config/JheraldBootSound/startup.wav")
 *   out_fmt         : [salida] valores del chunk "fmt " (para diagnostico)
 *   out_buf         : [salida] SOLO si JBS_OK -> buffer memalign(0x1000, out_alloc_size)
 *                     con el PCM al principio y ceros hasta out_alloc_size.
 *                     El llamador debe free() este puntero.
 *   out_alloc_size  : [salida] ALIGN_UP(pcm_size, 0x1000)  (tamano reservado)
 *   out_pcm_size    : [salida] bytes reales de PCM = data_size del chunk "data"
 *                     (NO se modifica para hacerlo multiplo de 0x1000)
 *
 * Devuelve JBS_OK o el estado de error. En error, *out_buf queda NULL y no hay
 * nada que liberar. Nunca aborta.
 */
jbs_status_t jbs_load_startup_pcm(const char *path,
                                  jbs_wav_fmt_t *out_fmt,
                                  void **out_buf,
                                  size_t *out_alloc_size,
                                  uint64_t *out_pcm_size);

#endif /* JBS_SDFILE_H */
