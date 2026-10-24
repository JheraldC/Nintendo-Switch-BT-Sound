/*
 * audio.c  -  reproduccion de un buffer PCM via audout (FASE 3.3)
 *
 * FASE 3.3 -- corrige un fallo real observado en hardware (FASE 3.1): la
 * consulta a AtmosphereHasService (cmd 65100 sobre "sm:") devolvia
 * AUDIO_SERVICE_QUERY_FAILED en el primer intento porque se enviaba con
 * serializacion CMIF (serviceDispatchInOut + smGetServiceSession()). En
 * libnx moderno, la sesion "sm:" opera en TIPC cuando
 * hosversionIsAtmosphere() || hosversionAtLeast(12,0,0) -- exactamente el
 * caso de esta consola (Atmosphere + HOS reciente). La correccion sustituye
 * UNICAMENTE el wrapper de la consulta por tipcDispatchInOut() sobre
 * smGetServiceSessionTipc(). Nada mas de este archivo cambia de logica.
 *
 * Nota de implementacion (verificada en switch/sf/tipc.h, no asumida):
 * tipcMakeRequest() suma internamente 16 al request_id antes de ponerlo en
 * el campo "type" del mensaje TIPC (los valores 0-15 estan reservados para
 * mensajes de control, p.ej. TipcCommandType_Close=15). Por eso se sigue
 * pasando el mismo numero logico de comando, 65100 -- igual que en la
 * interfaz CMIF -- y es la propia macro tipcDispatchInOut() quien aplica esa
 * traduccion; NO hay que sumar/restar nada a mano.
 *
 * Espera de audout:u -- SIN hilo trabajador (ver docs/DESIGN.md, correccion
 * FASE 3.1): Atmosphere extiende "sm:" (la misma sesion que ya abre
 * smInitialize(), NO sm:m ni ningun otro servicio) con el comando IPC 65100,
 * "AtmosphereHasService(ServiceName) -> bool", documentado en
 * docs/components/modules/sm.md del repositorio de Atmosphere. Se consulta
 * en un bucle de polling ligero (deadline monotonico de 8 s, ~20 ms entre
 * intentos) SIN bloquear en ninguna llamada. Solo cuando el servicio ya
 * existe se llama a audoutInitialize() -- UNA sola vez.
 */
#include "audio.h"
#include "wav.h"

#include <switch.h>
#include <string.h>

/* Plazo maximo para que audout:u este disponible tras invocar jbs_audio_play(). */
#define JBS_AUDIO_SERVICE_TIMEOUT_NS   (8000000000ull)     /* 8 s */
/* Intervalo entre sondeos de AtmosphereHasService("audout:u"). */
#define JBS_AUDIO_POLL_SLEEP_NS        (20000000ull)       /* 20 ms */
/* Margen sobre la duracion teorica del PCM para el timeout de WaitPlayFinish. */
#define JBS_PLAYBACK_TIMEOUT_MARGIN_NS (2000000000ull)     /*  2 s */
/* Limite superior duro del timeout de reproduccion. FASE 4.0 subio
 * JBS_MAX_PCM_SIZE a 2 MiB (~10,92 s de audio); con el margen de 2 s el peor
 * caso teorico son ~12,92 s, asi que el limite duro sube a 15 s para
 * mantener holgura real por encima del peor caso posible (nunca deberia
 * activarse en uso normal, es una red de seguridad ante datos inconsistentes). */
#define JBS_PLAYBACK_TIMEOUT_MAX_NS    (15000000000ull)    /* 15 s */

/* Comando IPC de la extension de Atmosphere sobre "sm:" (docs/components/modules/sm.md
 * del repositorio Atmosphere-NX/Atmosphere): AtmosphereHasService(ServiceName) -> bool. */
#define JBS_ATMOSPHERE_HAS_SERVICE_CMD 65100u

/* sizeof(bool) debe ser 1 byte para que el campo de salida IPC (1 byte segun
 * la interfaz sf::Out<bool> de Atmosphere) se corresponda exactamente con lo
 * que tipcDispatchInOut() copia. En la ABI aarch64 usada por devkitA64 esto
 * siempre es cierto; se deja como comprobacion explicita, no como supuesto. */
_Static_assert(sizeof(bool) == 1, "AtmosphereHasService espera un bool de 1 byte");

static uint64_t ns_to_ms(uint64_t ns) { return ns / 1000000ull; }

/*
 * jbs_sm_has_service: true/false en *out_available si el servicio ya esta
 * registrado en sm. NO abre ninguna sesion nueva (usa
 * smGetServiceSessionTipc(), sobre la misma conexion que __appInit ya
 * inicializo via smInitialize()) y NO amplia el SAC.
 *
 * FASE 3.3: enviado por TIPC (tipcDispatchInOut + smGetServiceSessionTipc),
 * NO por CMIF. La version anterior (serviceDispatchInOut + smGetServiceSession,
 * es decir CMIF) fallaba en hardware real con este comando -- ver cabecera.
 */
static Result jbs_sm_has_service(const char *service_name, bool *out_available)
{
    SmServiceName name = smEncodeName(service_name);
    bool out = false;

    Result rc = tipcDispatchInOut(smGetServiceSessionTipc(), JBS_ATMOSPHERE_HAS_SERVICE_CMD,
                                  name, out);

    *out_available = R_SUCCEEDED(rc) && out;
    return rc;
}

/*
 * Espera a que audout:u exista (polling ligero, deadline duro de 8 s) y,
 * solo entonces, llama a audoutInitialize() una unica vez. Rellena en 'out'
 * el diagnostico fiel de cada paso (ran + Result code).
 *
 * Devuelve:
 *   JBS_OK                          -> audout:u encontrado, audoutInitialize() con exito
 *   JBS_AUDIO_SERVICE_QUERY_FAILED  -> la propia consulta 65100 fallo (no es timeout)
 *   JBS_AUDIO_SERVICE_TIMEOUT       -> paso el deadline y el servicio nunca aparecio
 *   JBS_AUDIO_INIT_FAILED           -> el servicio SI aparecio pero audoutInitialize() fallo
 */
static jbs_status_t audio_wait_and_init(jbs_audio_result_t *out)
{
    uint64_t t0 = armGetSystemTick();

    for (;;) {
        bool available = false;

        out->service_query_ran = 1;
        Result rc = jbs_sm_has_service("audout:u", &available);
        out->service_query_rc = (uint32_t)rc;

        if (R_FAILED(rc)) {
            out->audio_wait_ms = ns_to_ms(armTicksToNs(armGetSystemTick() - t0));
            return JBS_AUDIO_SERVICE_QUERY_FAILED;
        }
        if (available)
            break;

        uint64_t elapsed_ns = armTicksToNs(armGetSystemTick() - t0);
        if (elapsed_ns >= JBS_AUDIO_SERVICE_TIMEOUT_NS) {
            out->audio_wait_ms = ns_to_ms(elapsed_ns);
            return JBS_AUDIO_SERVICE_TIMEOUT;
        }
        svcSleepThread(JBS_AUDIO_POLL_SLEEP_NS);
    }

    out->audio_wait_ms = ns_to_ms(armTicksToNs(armGetSystemTick() - t0));

    /* audout:u ya existe: UNA sola llamada, sin reintentos. */
    out->audout_init_ran = 1;
    Result irc = audoutInitialize();
    out->audout_init_rc = (uint32_t)irc;
    if (R_FAILED(irc))
        return JBS_AUDIO_INIT_FAILED;

    return JBS_OK;
}

/* ---------------------------------------------------------------------- */
/* Reproduccion (sin cambios de logica respecto a FASE 3.1: formato,       */
/* buffer, timeout de reproduccion y orden de cleanup)                    */
/* ---------------------------------------------------------------------- */

jbs_status_t jbs_audio_play(void *pcm_buffer, uint64_t allocation_size, uint64_t pcm_size,
                            jbs_audio_result_t *out_result)
{
    jbs_status_t result = JBS_OK;
    bool audout_ready = false;
    bool started = false;
    uint64_t t0 = armGetSystemTick();

    memset(out_result, 0, sizeof(*out_result));   /* todo *_ran queda en 0 = NOT_RUN */

    /* 1) esperar audout:u por polling (AtmosphereHasService/TIPC) + init una sola vez */
    jbs_status_t wait_st = audio_wait_and_init(out_result);
    if (wait_st != JBS_OK)
        return wait_st;   /* nada que limpiar todavia: audout no llego a inicializarse */
    audout_ready = true;

    /* 2) verificar que el dispositivo coincide EXACTAMENTE con lo validado en FASE 2/2.1 */
    out_result->device_sample_rate = audoutGetSampleRate();
    out_result->device_channels    = audoutGetChannelCount();
    out_result->device_format      = (uint32_t)audoutGetPcmFormat();

    if (out_result->device_sample_rate != JBS_WAV_SAMPLE_RATE ||
        out_result->device_channels    != JBS_WAV_CHANNELS    ||
        out_result->device_format      != (uint32_t)PcmFormat_Int16) {
        result = JBS_AUDIO_DEVICE_MISMATCH;
        goto cleanup;
    }

    /* 3) start */
    out_result->start_ran = 1;
    {
        Result src = audoutStartAudioOut();
        out_result->start_rc = (uint32_t)src;
        if (R_FAILED(src)) {
            result = JBS_AUDIO_START_FAILED;
            goto cleanup;
        }
    }
    started = true;

    /* 4) un unico AudioOutBuffer. pcm_size NUNCA se redondea; allocation_size
     *    y el propio puntero ya vienen alineados a 0x1000 desde sdfile.c. */
    {
        AudioOutBuffer ab;
        memset(&ab, 0, sizeof(ab));
        ab.next        = NULL;
        ab.buffer      = pcm_buffer;
        ab.buffer_size = allocation_size;
        ab.data_size   = pcm_size;
        ab.data_offset = 0;

        uint64_t t_play = armGetSystemTick();

        out_result->append_ran = 1;
        Result arc = audoutAppendAudioOutBuffer(&ab);
        out_result->append_rc = (uint32_t)arc;
        if (R_FAILED(arc)) {
            result = JBS_AUDIO_APPEND_FAILED;
            goto cleanup;
        }
        out_result->playback_start_ms = ns_to_ms(armTicksToNs(t_play - t0));

        /* 5) timeout de reproduccion = duracion teorica + margen, acotado */
        uint64_t frames = pcm_size / JBS_WAV_BLOCK_ALIGN;              /* pcm_size ya validado
                                                                          multiplo de 4 en FASE 2 */
        uint64_t duration_ns = (frames * 1000000000ull) / JBS_WAV_SAMPLE_RATE;
        out_result->estimated_duration_ms = ns_to_ms(duration_ns);   /* diagnostico: teorica vs medida */
        uint64_t timeout_ns = duration_ns + JBS_PLAYBACK_TIMEOUT_MARGIN_NS;
        if (timeout_ns > JBS_PLAYBACK_TIMEOUT_MAX_NS)
            timeout_ns = JBS_PLAYBACK_TIMEOUT_MAX_NS;

        AudioOutBuffer *released = NULL;
        u32 released_count = 0;

        out_result->wait_ran = 1;
        Result wprc = audoutWaitPlayFinish(&released, &released_count, timeout_ns);
        out_result->wait_rc = (uint32_t)wprc;

        out_result->playback_duration_ms = ns_to_ms(armTicksToNs(armGetSystemTick() - t_play));
        out_result->released_count = released_count;

        if (R_FAILED(wprc)) {
            result = (R_VALUE(wprc) == KERNELRESULT(TimedOut)) ? JBS_PLAYBACK_TIMEOUT
                                                                : JBS_PLAYBACK_FAILED;
            goto cleanup;
        }
    }

cleanup:
    /* Ruta unica para CUALQUIER camino de salida: sin hilos que abandonar,
     * sin threadClose(), sin carreras con el __appExit() de main.c. */
    if (started) {
        out_result->stop_ran = 1;
        Result stoprc = audoutStopAudioOut();
        out_result->stop_rc = (uint32_t)stoprc;
        if (R_FAILED(stoprc) && result == JBS_OK)
            result = JBS_AUDIO_STOP_FAILED;
    }
    if (audout_ready) {
        audoutExit();
    }
    return result;
}
