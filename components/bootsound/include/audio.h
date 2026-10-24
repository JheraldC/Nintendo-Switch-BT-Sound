/*
 * audio.h  -  reproduccion de UN buffer PCM via audout (FASE 3.3)
 *
 * Responsabilidad exclusiva de este modulo: esperar a que audout:u este
 * disponible (polling ligero con deadline duro de 8 s, vía la extension de
 * Atmosphere AtmosphereHasService sobre "sm:", enviada por TIPC -- ver
 * source/audio.c), inicializar audout UNA vez, verificar que el dispositivo
 * coincide con el formato ya validado por wav.c/sdfile.c, reproducir el
 * buffer, esperar a que termine (con timeout) y cerrar todo.
 *
 * NO contiene logica de parsing WAV ni de filesystem (eso es wav.c/sdfile.c).
 * NO crea hilos ni deja nada residente: jbs_audio_play() es una unica
 * secuencia lineal en el hilo principal; al volver, audout ya esta cerrado
 * (audoutExit ya ejecutado) pase lo que pase.
 */
#ifndef JBS_AUDIO_H
#define JBS_AUDIO_H

#include <stdint.h>
#include "wav.h"   /* jbs_status_t: se reutiliza el mismo enum de estados */

/*
 * Diagnostico fiel de tiempos, dispositivo y RESULT CODE de cada paso, para
 * phase3_result.txt. Todo a 0 al entrar (memset). Cada paso tiene su propio
 * flag "*_ran": SOLO si vale 1 esa operacion llego a ejecutarse de verdad, y
 * solo entonces su "*_rc" es significativo. Esto evita que el diagnostico
 * afirme "append=OK" (o cualquier otra cosa) cuando esa llamada nunca se hizo
 * -- el bug exacto que se detecto en la primera prueba de hardware de FASE 3.1.
 */
typedef struct {
    uint32_t device_sample_rate;      /* audoutGetSampleRate()   (0 si no se llego a leer) */
    uint32_t device_channels;         /* audoutGetChannelCount() */
    uint32_t device_format;           /* audoutGetPcmFormat(), como u32 (2 = PcmFormat_Int16) */
    uint64_t audio_wait_ms;           /* cuanto se tardo en tener audout:u listo (o en agotar el timeout) */
    uint64_t playback_start_ms;       /* desde el inicio de jbs_audio_play() hasta el Append */
    uint64_t playback_duration_ms;    /* desde el Append hasta que WaitPlayFinish retorna */
    uint64_t estimated_duration_ms;   /* duracion TEORICA calculada de pcm_size (FASE 4.0), misma
                                          formula que el timeout de WaitPlayFinish -- para comparar
                                          contra playback_duration_ms (lo medido de verdad) */
    uint32_t released_count;          /* buffers liberados devueltos por WaitPlayFinish (0 o 1) */

    /* Paso ejecutado (1) o no (0, "NOT_RUN") + Result code (solo valido si *_ran==1). */
    uint8_t  service_query_ran;  uint32_t service_query_rc;  /* ultimo AtmosphereHasService */
    uint8_t  audout_init_ran;    uint32_t audout_init_rc;    /* audoutInitialize() */
    uint8_t  start_ran;          uint32_t start_rc;          /* audoutStartAudioOut() */
    uint8_t  append_ran;         uint32_t append_rc;         /* audoutAppendAudioOutBuffer() */
    uint8_t  wait_ran;           uint32_t wait_rc;           /* audoutWaitPlayFinish() */
    uint8_t  stop_ran;           uint32_t stop_rc;           /* audoutStopAudioOut() */
} jbs_audio_result_t;

/*
 * jbs_audio_play: reproduce EXACTAMENTE un buffer PCM ya cargado y alineado.
 *
 *   pcm_buffer     : buffer alineado a 0x1000 (el que crea sdfile.c)
 *   allocation_size: tamano reservado, alineado a 0x1000 (buffer_size de AudioOutBuffer)
 *   pcm_size       : bytes REALES de PCM dentro del buffer (data_size de AudioOutBuffer;
 *                    NUNCA se redondea)
 *   out_result     : [salida] diagnostico de tiempos/dispositivo/Result codes por paso,
 *                    con NOT_RUN fiel para los pasos que no llegaron a ejecutarse
 *                    (memset a 0 al entrar)
 *
 * Devuelve JBS_OK si se reprodujo el buffer completo, o el estado de error
 * correspondiente. Nunca aborta, nunca bloquea indefinidamente: internamente
 * usa un deadline duro para esperar audout:u y un timeout acotado para
 * WaitPlayFinish (ver DESIGN.md, seccion FASE 3, para las formulas exactas).
 *
 * Garantia de cleanup: en CUALQUIER camino de salida, si audoutStartAudioOut()
 * llego a tener exito se llama a audoutStopAudioOut(); si audoutInitialize()
 * llego a tener exito se llama a audoutExit(). pcm_buffer NUNCA se libera
 * dentro de esta funcion (responsabilidad del llamador) y nunca se toca tras
 * haber cerrado audout.
 */
jbs_status_t jbs_audio_play(void *pcm_buffer, uint64_t allocation_size, uint64_t pcm_size,
                            jbs_audio_result_t *out_result);

#endif /* JBS_AUDIO_H */
