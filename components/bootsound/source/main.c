/*
 * JheraldBootSound  -  sysmodule boot2, NO residente.
 *
 * ================  FASE 4.0  -  AUDIO PERSONALIZABLE + config.ini + 2 MiB  ================
 *
 * Flujo:
 *   boot2 -> main()
 *     -> fsdevMountSdmc()
 *     -> leer config.ini (si existe) -> resolver que archivo intentar primero  [config.c]
 *          (por defecto, o si config.ini no existe / audio_file no es seguro: startup.wav)
 *     -> localizar/validar/cargar  sdmc:/config/JheraldBootSound/<resuelto>
 *          (RIFF/WAVE, PCM 48000/16/stereo LE, hasta 2 MiB)  a un buffer memalign(0x1000,.) [wav.c/sdfile.c]
 *     -> si esa carga falla Y el resuelto no era ya "startup.wav": reintentar
 *          una unica vez con startup.wav (fallback)                                          [main.c]
 *     -> esperar audout:u por POLLING (AtmosphereHasService, cmd 65100 sobre "sm:",
 *          deadline duro 8 s, sin hilos) -> inicializar UNA vez -> verificar dispositivo
 *          -> reproducir el buffer -> esperar a que termine (timeout) -> cerrar             [audio.c]
 *     -> [si existe debug.flag] escribir phase4_result.txt
 *     -> free(buffer)   (audout ya cerrado en este punto: jbs_audio_play() no
 *                         retorna hasta haber hecho Stop+Exit en TODOS sus caminos)
 *     -> fsdevUnmountDevice("sdmc")
 *     -> return 0   (SIEMPRE)  -> HOME continua normalmente
 *
 * La espera de audout:u (AtmosphereHasService/TIPC, polling 20 ms, deadline 8 s) es
 * EXACTAMENTE la de FASE 3.3, ya validada en hardware -- FASE 4.0 no la toca.
 *
 * Fail-safe absoluto: cualquier error -> cleanup -> return 0. Nunca fatalThrow /
 * abort / diagAbort / svcBreak. Ningun fallo de audio ni de config.ini impide
 * llegar a HOME. No hay reproduccion en bucle, ni daemon, ni hilos NUNCA.
 */

#include <switch.h>
#include <stdlib.h>
#include <string.h>

#include "wav.h"
#include "sdfile.h"
#include "audio.h"
#include "config.h"
#include "debug.h"
#include "jbs_paths.h"

/* -------------------------------------------------------------------------- */
/* Boilerplate de sysmodule libnx                                            */
/* -------------------------------------------------------------------------- */

u32 __nx_applet_type = AppletType_None;   /* un sysmodule no usa la applet API */
u32 __nx_fs_num_sessions = 1;             /* 1 sesion de fsp-srv es suficiente  */

/*
 * Heap interno propio. Recalculado para FASE 4.0 (JBS_MAX_PCM_SIZE 1 MiB -> 2 MiB):
 *   - buffer PCM final:  hasta ALIGN_UP(JBS_MAX_PCM_SIZE, 0x1000) = 2 MiB
 *   - margen para libnx + newlib + fsdev + 1 FILE + stat/structs + audout
 *     (Service/AudioOutBuffer, todo en pila) + el parser de config.ini
 *     (buffer de linea de 256 B, TAMBIEN en pila, no en este heap): 0x80000 (512 KiB)
 *   Total: 0x280000 (2,5 MiB) reservado en .bss (no engorda el NSO; RAM ~1-15 s).
 *
 * El margen (0x80000) NO cambia respecto a FASE 3.1/3.3: el parser de
 * config.ini no añade consumo de heap real (usa un buffer de linea en la
 * pila del hilo principal, no memoria dinamica), asi que el mismo margen que
 * ya cubria holgadamente newlib+fsdev+audout sigue siendo suficiente; solo
 * escala el termino del buffer PCM, que ya estaba parametrizado.
 *
 * REGLA: si algun dia se sube JBS_MAX_PCM_SIZE (wav.h), subir este valor en la
 * misma cantidad. El _Static_assert de abajo lo verifica en compilacion.
 */
#define INNER_HEAP_SIZE 0x280000

_Static_assert(INNER_HEAP_SIZE >= JBS_ALIGN_UP(JBS_MAX_PCM_SIZE, JBS_PCM_ALIGN) + 0x80000,
               "INNER_HEAP_SIZE debe cubrir el buffer PCM maximo + margen de runtime");

size_t nx_inner_heap_size = INNER_HEAP_SIZE;
char   nx_inner_heap[INNER_HEAP_SIZE];

void __libnx_initheap(void)
{
    extern char *fake_heap_start;
    extern char *fake_heap_end;
    fake_heap_start = nx_inner_heap;
    fake_heap_end   = nx_inner_heap + nx_inner_heap_size;
}

void __appInit(void)
{
    /* Init minima y tolerante a fallos: NO abortamos nunca. audout NO se
     * inicializa aqui (ver audio.c: se hace con deadline, dentro de
     * jbs_audio_play(), solo despues de tener el PCM ya cargado). */
    smInitialize();
    fsInitialize();
}

void __appExit(void)
{
    fsExit();
    smExit();
}

/* -------------------------------------------------------------------------- */
/* Orquestacion FASE 4.0                                                      */
/* -------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    jbs_status_t        st         = JBS_OK;
    jbs_wav_fmt_t        fmt;
    jbs_audio_result_t   audio_res;
    jbs_config_result_t  cfg;
    void                *pcm        = NULL;
    size_t               alloc_sz   = 0;
    uint64_t             pcm_sz     = 0;
    int                  sd_mounted = 0;
    char                 audio_path[192];
    char                 selected_file[JBS_AUDIO_FILENAME_MAX];

    memset(&fmt, 0, sizeof(fmt));
    memset(&audio_res, 0, sizeof(audio_res));
    memset(&cfg, 0, sizeof(cfg));
    strncpy(selected_file, "startup.wav", sizeof(selected_file) - 1);
    selected_file[sizeof(selected_file) - 1] = '\0';

    /* 1) montar la SD */
    if (R_FAILED(fsdevMountSdmc())) {
        st = JBS_SD_MOUNT_FAILED;
        goto cleanup;
    }
    sd_mounted = 1;

    /* 2) resolver que archivo intentar primero (config.ini, si existe) */
    jbs_config_load(JBS_PATH_CONFIG, &cfg);

    if (jbs_config_build_path(audio_path, sizeof(audio_path), cfg.resolved) != 0) {
        /* No deberia ocurrir nunca (JBS_DIR + 127 bytes caben de sobra en 192),
         * pero si pasara, usar el default fijo directamente en vez de arriesgar
         * una ruta truncada/incorrecta. */
        strncpy(cfg.resolved, "startup.wav", sizeof(cfg.resolved) - 1);
        cfg.resolved[sizeof(cfg.resolved) - 1] = '\0';
        strncpy(audio_path, JBS_PATH_WAV, sizeof(audio_path) - 1);
        audio_path[sizeof(audio_path) - 1] = '\0';
    }
    strncpy(selected_file, cfg.resolved, sizeof(selected_file) - 1);
    selected_file[sizeof(selected_file) - 1] = '\0';

    /* 3) localizar + validar RIFF/WAVE + cargar PCM en buffer alineado */
    st = jbs_load_startup_pcm(audio_path, &fmt, &pcm, &alloc_sz, &pcm_sz);

    /* 3b) fallback en tiempo de ejecucion: si el archivo resuelto (personalizado
     *     o no) fallo por CUALQUIER motivo y no era ya el default, reintentar
     *     una unica vez con startup.wav. 'selected_file' pasa a reflejar el
     *     ULTIMO archivo realmente intentado, tenga o no exito ese intento. */
    if (st != JBS_OK && strcmp(cfg.resolved, "startup.wav") != 0) {
        strncpy(selected_file, "startup.wav", sizeof(selected_file) - 1);
        selected_file[sizeof(selected_file) - 1] = '\0';

        memset(&fmt, 0, sizeof(fmt));
        st = jbs_load_startup_pcm(JBS_PATH_WAV, &fmt, &pcm, &alloc_sz, &pcm_sz);
    }

    if (st != JBS_OK)
        goto cleanup;
    /* st == JBS_OK  <=>  pcm != NULL con pcm_sz bytes de PCM y alloc_sz reservados. */

    /* 4) esperar audout:u -> inicializar -> verificar -> reproducir -> cerrar.
     *    jbs_audio_play() garantiza que, al retornar (por cualquier camino),
     *    audout ya esta cerrado si llego a abrirse. Mecanismo de espera SIN
     *    cambios respecto a FASE 3.3 (ya validada en hardware). */
    st = jbs_audio_play(pcm, (uint64_t)alloc_sz, pcm_sz, &audio_res);

cleanup:
    /* 5) diagnostico TEMPORAL: solo si existe debug.flag. Archivo DISTINTO de
     *    phase2_result.txt y phase3_result.txt (nunca se tocan ni se borran). */
    if (sd_mounted && jbs_debug_enabled(JBS_PATH_DEBUGFLAG)) {
        jbs_debug_write_phase4_result(JBS_PATH_RESULT4, st, &fmt,
                                      pcm_sz, (uint64_t)alloc_sz, &audio_res,
                                      &cfg, selected_file);
    }

    /* 6) liberar recursos (simetrico, en orden inverso). audout ya esta cerrado
     *    en este punto (ver paso 4), asi que liberar el buffer aqui es seguro. */
    if (pcm != NULL) {
        free(pcm);
        pcm = NULL;
    }
    if (sd_mounted) {
        fsdevUnmountDevice("sdmc");
    }

    /* 7) terminar SIEMPRE con exito: el arranque no depende de esto. */
    return 0;
}
