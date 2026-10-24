/*
 * debug.h  -  diagnostico TEMPORAL de desarrollo (FASE 2), activado por archivo marcador.
 *
 * Mecanismo (opcion B): si existe  sdmc:/config/JheraldBootSound/debug.flag
 *   -> se escribe  sdmc:/config/JheraldBootSound/phase2_result.txt
 * Si NO existe debug.flag  -> no se escribe absolutamente nada.
 *
 * Sin sockets, sin telemetria, sin logger residente. La escritura del resultado
 * es best-effort: si falla, se ignora (nunca afecta al arranque).
 */
#ifndef JBS_DEBUG_H
#define JBS_DEBUG_H

#include <stdint.h>
#include "wav.h"
#include "audio.h"
#include "config.h"

/* true si existe el archivo marcador (flag_path). */
int jbs_debug_enabled(const char *flag_path);

/*
 * Escribe/actualiza el archivo de resultado de FASE 2/2.1. Llamar SOLO si
 * jbs_debug_enabled(). No se usa a partir de FASE 3 (ver
 * jbs_debug_write_phase3_result); se conserva sin cambios por compatibilidad.
 *   out_path   : ruta de phase2_result.txt
 *   st         : estado final de la fase
 *   fmt        : valores leidos de "fmt " (puede ser NULL o todo a cero)
 *   pcm_bytes  : bytes reales de PCM cargados (0 si no se llego a cargar)
 *   alloc_bytes: bytes reservados = ALIGN_UP(pcm_bytes, 0x1000) (0 si no se reservo)
 */
void jbs_debug_write_result(const char *out_path,
                            jbs_status_t st,
                            const jbs_wav_fmt_t *fmt,
                            uint64_t pcm_bytes,
                            uint64_t alloc_bytes);

/*
 * Escribe/actualiza el archivo de resultado de FASE 3 (audout). Llamar SOLO
 * si jbs_debug_enabled(). Archivo DISTINTO de phase2_result.txt (nunca lo
 * sustituye ni lo borra).
 *   out_path   : ruta de phase3_result.txt
 *   st         : estado final de la fase (parser+filesystem+audio, el primero que falle)
 *   fmt        : valores leidos de "fmt " (puede ser NULL o todo a cero)
 *   pcm_bytes  : bytes reales de PCM cargados (0 si no se llego a cargar)
 *   alloc_bytes: bytes reservados = ALIGN_UP(pcm_bytes, 0x1000)
 *   audio      : diagnostico de audio.c (puede ser NULL o todo a cero si no se llego a intentar)
 */
void jbs_debug_write_phase3_result(const char *out_path,
                                   jbs_status_t st,
                                   const jbs_wav_fmt_t *fmt,
                                   uint64_t pcm_bytes,
                                   uint64_t alloc_bytes,
                                   const jbs_audio_result_t *audio);

/*
 * Escribe/actualiza el archivo de resultado de FASE 4.0 (config.ini + audio
 * personalizable). Llamar SOLO si jbs_debug_enabled(). Archivo DISTINTO de
 * phase2_result.txt y phase3_result.txt (nunca los sustituye ni los borra).
 *   out_path        : ruta de phase4_result.txt
 *   st              : estado final (parser+filesystem+audio, el primero que falle)
 *   fmt             : valores leidos de "fmt " (puede ser NULL o todo a cero)
 *   pcm_bytes       : bytes reales de PCM cargados del archivo FINALMENTE usado
 *   alloc_bytes     : bytes reservados = ALIGN_UP(pcm_bytes, 0x1000)
 *   audio           : diagnostico de audio.c (puede ser NULL o todo a cero)
 *   cfg             : resultado de jbs_config_load() (config.ini)
 *   selected_file   : nombre de archivo REALMENTE usado en el ultimo intento
 *                      (== cfg->resolved si no hubo fallback en tiempo de
 *                      ejecucion; "startup.wav" si lo hubo)
 */
void jbs_debug_write_phase4_result(const char *out_path,
                                   jbs_status_t st,
                                   const jbs_wav_fmt_t *fmt,
                                   uint64_t pcm_bytes,
                                   uint64_t alloc_bytes,
                                   const jbs_audio_result_t *audio,
                                   const jbs_config_result_t *cfg,
                                   const char *selected_file);

#endif /* JBS_DEBUG_H */
