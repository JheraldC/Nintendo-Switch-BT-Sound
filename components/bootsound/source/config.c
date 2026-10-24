/*
 * config.c  -  lector minimo de config.ini (FASE 4.0)
 * Portable: solo <stdio.h>/<string.h>. Sin <switch.h>. Se prueba en PC.
 */
#include "config.h"
#include "jbs_paths.h"   /* JBS_DIR */

#include <stdio.h>
#include <string.h>

/* Suficiente para "audio_file" + " = " + un valor deliberadamente largo (para
 * poder DETECTAR y rechazar un nombre demasiado largo, no truncarlo en
 * silencio a algo mas corto que "por casualidad" pareciera valido). */
#define JBS_CONFIG_LINE_MAX 256u

const char *jbs_config_status_str(jbs_config_status_t s)
{
    switch (s) {
        case JBS_CONFIG_NOT_FOUND:          return "NOT_FOUND";
        case JBS_CONFIG_OK:                 return "OK";
        case JBS_CONFIG_INVALID_AUDIO_FILE: return "INVALID_AUDIO_FILE";
        default:                            return "UNKNOWN";
    }
}

int jbs_config_filename_is_safe(const char *filename)
{
    size_t len, i;

    if (!filename)
        return 0;
    len = strlen(filename);
    if (len == 0 || len >= JBS_AUDIO_FILENAME_MAX)
        return 0;

    for (i = 0; i < len; i++) {
        char c = filename[i];
        if (c == '/' || c == '\\' || c == ':')
            return 0;
    }
    if (strstr(filename, "..") != NULL)
        return 0;

    return 1;
}

int jbs_config_build_path(char *out, size_t out_size, const char *filename)
{
    int n = snprintf(out, out_size, JBS_DIR "/%s", filename);
    if (n < 0 || (size_t)n >= out_size)
        return -1;   /* se habria truncado */
    return 0;
}

static char *skip_leading_ws(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

static void trim_trailing_ws(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
}

void jbs_config_load(const char *config_path, jbs_config_result_t *out)
{
    FILE *f;
    char line[JBS_CONFIG_LINE_MAX];

    memset(out, 0, sizeof(*out));
    strncpy(out->resolved, "startup.wav", sizeof(out->resolved) - 1);
    out->resolved[sizeof(out->resolved) - 1] = '\0';

    f = fopen(config_path, "r");
    if (!f) {
        out->status = JBS_CONFIG_NOT_FOUND;
        return;
    }

    while (fgets(line, (int)sizeof(line), f)) {
        char *p, *eq, *key, *val;
        size_t n;

        /* recortar salto de linea final (\n, \r\n) */
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }

        p = skip_leading_ws(line);
        if (*p == '\0' || *p == '#' || *p == ';' || *p == '[')
            continue;   /* vacia, comentario, o cabecera de seccion: ignorar */

        eq = strchr(p, '=');
        if (!eq)
            continue;   /* sin '=': linea no reconocida, se ignora (defensivo) */

        *eq = '\0';
        key = p;
        val = skip_leading_ws(eq + 1);
        trim_trailing_ws(key);
        trim_trailing_ws(val);

        if (strcmp(key, "audio_file") == 0) {
            out->has_audio_file = 1;

            /* 'configured' es solo para diagnostico: se copia (y trunca si
             * hiciera falta) SIN que esa copia influya en la validacion,
             * que se hace sobre 'val' completo, sin truncar. */
            strncpy(out->configured, val, sizeof(out->configured) - 1);
            out->configured[sizeof(out->configured) - 1] = '\0';

            if (jbs_config_filename_is_safe(val)) {
                out->status = JBS_CONFIG_OK;
                strncpy(out->resolved, val, sizeof(out->resolved) - 1);
                out->resolved[sizeof(out->resolved) - 1] = '\0';
            } else {
                out->status = JBS_CONFIG_INVALID_AUDIO_FILE;
                /* 'resolved' se queda en el default "startup.wav" de arriba */
            }
        }
        /* claves desconocidas: se ignoran */
    }
    fclose(f);

    if (!out->has_audio_file)
        out->status = JBS_CONFIG_OK;   /* config.ini existe, sin audio_file -> default, no es error */
}
