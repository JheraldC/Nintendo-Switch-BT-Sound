/*
 * debug.c  -  diagnostico temporal por archivo marcador (FASE 2, 3 y 4). Best-effort.
 * Portable: solo stdio + sys/stat.
 */
#include "debug.h"
#include "jbs_paths.h"

#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <string.h>

int jbs_debug_enabled(const char *flag_path)
{
    struct stat st;
    return (stat(flag_path, &st) == 0) ? 1 : 0;
}

void jbs_debug_write_result(const char *out_path,
                            jbs_status_t st,
                            const jbs_wav_fmt_t *fmt,
                            uint64_t pcm_bytes,
                            uint64_t alloc_bytes)
{
    FILE *f = fopen(out_path, "wb");
    if (!f)
        return;   /* best-effort: si no se puede escribir, no pasa nada */

    fprintf(f, "JheraldBootSound FASE 2\n");
    fprintf(f, "version=%s\n", JBS_VERSION_STR);
    fprintf(f, "status=%s\n", jbs_status_str(st));

    if (fmt) {
        fprintf(f, "audio_format=%u\n",    (unsigned)fmt->audio_format);
        fprintf(f, "sample_rate=%u\n",     (unsigned)fmt->sample_rate);
        fprintf(f, "channels=%u\n",        (unsigned)fmt->num_channels);
        fprintf(f, "bits_per_sample=%u\n", (unsigned)fmt->bits_per_sample);
        fprintf(f, "block_align=%u\n",     (unsigned)fmt->block_align);
        fprintf(f, "byte_rate=%u\n",       (unsigned)fmt->byte_rate);
    }
    fprintf(f, "pcm_bytes=%" PRIu64 "\n",   pcm_bytes);
    fprintf(f, "alloc_bytes=%" PRIu64 "\n", alloc_bytes);

    if (st == JBS_OK)
        fprintf(f, "format=PCM\n");

    fclose(f);
}

/* "ran" (0/1) -> "NOT_RUN", o "OK"/"FAILED" segun R_FAILED(rc). Nunca afirma
 * OK/FAILED de un paso que no se ejecuto de verdad (ver audio.h, FASE 3.3). */
static const char *step_str(uint8_t ran, uint32_t rc)
{
    if (!ran) return "NOT_RUN";
    return (rc != 0) ? "FAILED" : "OK";
}

void jbs_debug_write_phase3_result(const char *out_path,
                                   jbs_status_t st,
                                   const jbs_wav_fmt_t *fmt,
                                   uint64_t pcm_bytes,
                                   uint64_t alloc_bytes,
                                   const jbs_audio_result_t *audio)
{
    FILE *f = fopen(out_path, "wb");
    if (!f)
        return;   /* best-effort: si no se puede escribir, no pasa nada */

    fprintf(f, "JheraldBootSound FASE 3\n");
    fprintf(f, "version=%s\n", JBS_VERSION_STR);
    fprintf(f, "status=%s\n", jbs_status_str(st));

    if (fmt) {
        fprintf(f, "audio_format=%u\n",    (unsigned)fmt->audio_format);
        fprintf(f, "sample_rate=%u\n",     (unsigned)fmt->sample_rate);
        fprintf(f, "channels=%u\n",        (unsigned)fmt->num_channels);
        fprintf(f, "bits_per_sample=%u\n", (unsigned)fmt->bits_per_sample);
    }
    fprintf(f, "pcm_bytes=%" PRIu64 "\n",   pcm_bytes);
    fprintf(f, "alloc_bytes=%" PRIu64 "\n", alloc_bytes);

    if (audio) {
        /* Cada paso: estado fiel (NOT_RUN si no llego a ejecutarse) + Result code
         * en hex, solo con sentido cuando el paso si se ejecuto. */
        fprintf(f, "service_query=%s\n", step_str(audio->service_query_ran, audio->service_query_rc));
        fprintf(f, "service_query_rc=0x%08x\n", (unsigned)audio->service_query_rc);

        fprintf(f, "audout_init=%s\n", step_str(audio->audout_init_ran, audio->audout_init_rc));
        fprintf(f, "audout_init_rc=0x%08x\n", (unsigned)audio->audout_init_rc);

        fprintf(f, "device_sample_rate=%u\n", (unsigned)audio->device_sample_rate);
        fprintf(f, "device_channels=%u\n",    (unsigned)audio->device_channels);
        fprintf(f, "device_format=%s\n",
                (audio->device_format == 2) ? "Int16" : "OTHER");
        fprintf(f, "audio_wait_ms=%" PRIu64 "\n",        audio->audio_wait_ms);
        fprintf(f, "playback_start_ms=%" PRIu64 "\n",    audio->playback_start_ms);
        fprintf(f, "playback_duration_ms=%" PRIu64 "\n", audio->playback_duration_ms);

        fprintf(f, "start=%s\n", step_str(audio->start_ran, audio->start_rc));
        fprintf(f, "start_rc=0x%08x\n", (unsigned)audio->start_rc);

        fprintf(f, "append=%s\n", step_str(audio->append_ran, audio->append_rc));
        fprintf(f, "append_rc=0x%08x\n", (unsigned)audio->append_rc);
        fprintf(f, "released_count=%u\n", (unsigned)audio->released_count);

        fprintf(f, "wait=%s\n", step_str(audio->wait_ran, audio->wait_rc));
        fprintf(f, "wait_rc=0x%08x\n", (unsigned)audio->wait_rc);

        fprintf(f, "stop=%s\n", step_str(audio->stop_ran, audio->stop_rc));
        fprintf(f, "stop_rc=0x%08x\n", (unsigned)audio->stop_rc);

        /* Resumen global: OK solo si todo el flujo termino bien; NOT_RUN si ni
         * siquiera se llego a intentar reproducir (p.ej. fallo antes de Append). */
        fprintf(f, "playback=%s\n",
                (st == JBS_OK) ? "OK" : (audio->wait_ran ? "FAILED" : "NOT_RUN"));
    } else {
        fprintf(f, "playback=NOT_RUN\n");
    }

    fclose(f);
}

void jbs_debug_write_phase4_result(const char *out_path,
                                   jbs_status_t st,
                                   const jbs_wav_fmt_t *fmt,
                                   uint64_t pcm_bytes,
                                   uint64_t alloc_bytes,
                                   const jbs_audio_result_t *audio,
                                   const jbs_config_result_t *cfg,
                                   const char *selected_file)
{
    FILE *f = fopen(out_path, "wb");
    if (!f)
        return;   /* best-effort: si no se puede escribir, no pasa nada */

    fprintf(f, "JheraldBootSound FASE 4\n");
    fprintf(f, "version=%s\n", JBS_VERSION_STR);
    fprintf(f, "status=%s\n", jbs_status_str(st));
    fprintf(f, "\n");

    /* --- seleccion de archivo de audio (FASE 4.0) --- */
    if (cfg) {
        fprintf(f, "configured_audio_file=%s\n",
                cfg->has_audio_file ? cfg->configured : "(none)");
        fprintf(f, "selected_audio_file=%s\n", selected_file ? selected_file : "(unknown)");
        fprintf(f, "config_status=%s\n", jbs_config_status_str(cfg->status));
        fprintf(f, "fallback_used=%s\n",
                (cfg->has_audio_file && selected_file &&
                 strcmp(cfg->configured, selected_file) != 0) ? "YES" : "NO");
    } else {
        fprintf(f, "configured_audio_file=(none)\n");
        fprintf(f, "selected_audio_file=%s\n", selected_file ? selected_file : "(unknown)");
        fprintf(f, "config_status=%s\n", jbs_config_status_str(JBS_CONFIG_NOT_FOUND));
        fprintf(f, "fallback_used=NO\n");
    }
    fprintf(f, "\n");

    if (fmt) {
        fprintf(f, "audio_format=%u\n",    (unsigned)fmt->audio_format);
        fprintf(f, "sample_rate=%u\n",     (unsigned)fmt->sample_rate);
        fprintf(f, "channels=%u\n",        (unsigned)fmt->num_channels);
        fprintf(f, "bits_per_sample=%u\n", (unsigned)fmt->bits_per_sample);
    }
    fprintf(f, "pcm_bytes=%" PRIu64 "\n",   pcm_bytes);
    fprintf(f, "alloc_bytes=%" PRIu64 "\n", alloc_bytes);
    if (audio)
        fprintf(f, "estimated_duration_ms=%" PRIu64 "\n", audio->estimated_duration_ms);
    fprintf(f, "\n");

    if (audio) {
        fprintf(f, "service_query=%s\n", step_str(audio->service_query_ran, audio->service_query_rc));
        fprintf(f, "service_query_rc=0x%08x\n", (unsigned)audio->service_query_rc);

        fprintf(f, "audout_init=%s\n", step_str(audio->audout_init_ran, audio->audout_init_rc));
        fprintf(f, "audout_init_rc=0x%08x\n", (unsigned)audio->audout_init_rc);
        fprintf(f, "\n");

        fprintf(f, "device_sample_rate=%u\n", (unsigned)audio->device_sample_rate);
        fprintf(f, "device_channels=%u\n",    (unsigned)audio->device_channels);
        fprintf(f, "device_format=%s\n",
                (audio->device_format == 2) ? "Int16" : "OTHER");
        fprintf(f, "\n");

        fprintf(f, "audio_wait_ms=%" PRIu64 "\n",        audio->audio_wait_ms);
        fprintf(f, "playback_start_ms=%" PRIu64 "\n",    audio->playback_start_ms);
        fprintf(f, "playback_duration_ms=%" PRIu64 "\n", audio->playback_duration_ms);
        fprintf(f, "\n");

        fprintf(f, "start=%s\n", step_str(audio->start_ran, audio->start_rc));
        fprintf(f, "start_rc=0x%08x\n", (unsigned)audio->start_rc);

        fprintf(f, "append=%s\n", step_str(audio->append_ran, audio->append_rc));
        fprintf(f, "append_rc=0x%08x\n", (unsigned)audio->append_rc);
        fprintf(f, "released_count=%u\n", (unsigned)audio->released_count);

        fprintf(f, "wait=%s\n", step_str(audio->wait_ran, audio->wait_rc));
        fprintf(f, "wait_rc=0x%08x\n", (unsigned)audio->wait_rc);

        fprintf(f, "stop=%s\n", step_str(audio->stop_ran, audio->stop_rc));
        fprintf(f, "stop_rc=0x%08x\n", (unsigned)audio->stop_rc);
        fprintf(f, "\n");

        fprintf(f, "playback=%s\n",
                (st == JBS_OK) ? "OK" : (audio->wait_ran ? "FAILED" : "NOT_RUN"));
    } else {
        fprintf(f, "playback=NOT_RUN\n");
    }

    fclose(f);
}
