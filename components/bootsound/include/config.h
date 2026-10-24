/*
 * config.h  -  lector minimo de config.ini (FASE 4.0)
 *
 * Responsabilidad exclusiva: leer sdmc:/config/JheraldBootSound/config.ini
 * (si existe) y resolver que nombre de archivo de audio intentar primero.
 * NO abre el archivo de audio en si (eso es sdfile.c) ni decide el fallback
 * en tiempo de ejecucion cuando el archivo resuelto no carga (eso es
 * main.c, que ya conoce el resultado de sdfile.c).
 *
 * Formato admitido (INI minimo, sin libreria):
 *   - lineas vacias: se ignoran
 *   - lineas que empiecen (tras espacios) por '#' o ';': comentario, se ignora
 *   - lineas que empiecen por '[': cabecera de seccion, se ignora tal cual
 *   - "clave = valor" con espacios opcionales alrededor de '='
 *   - claves desconocidas: se ignoran
 *   - unica clave reconocida por ahora: audio_file
 *
 * NO depende de <switch.h>: portable, se prueba en PC (ver tests/).
 */
#ifndef JBS_CONFIG_H
#define JBS_CONFIG_H

#include <stddef.h>

/* Longitud maxima (incluido el terminador nulo) del VALOR de audio_file,
 * tanto el leido de config.ini como el resuelto/usado finalmente. Es una
 * cota sobre el nombre de archivo, no sobre la ruta completa. */
#define JBS_AUDIO_FILENAME_MAX 128u

typedef enum {
    JBS_CONFIG_NOT_FOUND = 0,        /* config.ini no existe: se usa el default (startup.wav) */
    JBS_CONFIG_OK,                    /* config.ini existe; audio_file ausente o valido */
    JBS_CONFIG_INVALID_AUDIO_FILE     /* config.ini existe; audio_file presente pero NO seguro */
} jbs_config_status_t;

const char *jbs_config_status_str(jbs_config_status_t s);

typedef struct {
    jbs_config_status_t status;
    int  has_audio_file;                        /* 1 si config.ini tenia la clave audio_file */
    char configured[JBS_AUDIO_FILENAME_MAX];     /* valor crudo leido (vacio si has_audio_file==0) */
    char resolved[JBS_AUDIO_FILENAME_MAX];       /* nombre de archivo a intentar PRIMERO:
                                                    = configured si es valido; si no, "startup.wav" */
} jbs_config_result_t;

/*
 * jbs_config_load: lee config_path (si existe) y rellena 'out'. NUNCA falla
 * de forma fatal: cualquier problema (archivo ausente, audio_file no seguro,
 * linea rara, clave repetida) deja 'resolved' en un valor utilizable
 * ("startup.wav" por defecto). No comprueba que el archivo resuelto exista
 * de verdad -- de eso se encarga sdfile.c despues, y el fallback en tiempo
 * de ejecucion si ese intento falla lo decide main.c.
 */
void jbs_config_load(const char *config_path, jbs_config_result_t *out);

/*
 * jbs_config_build_path: construye "sdmc:/config/JheraldBootSound/<filename>"
 * en 'out', via snprintf con comprobacion de truncamiento.
 *   devuelve 0 si OK, -1 si el resultado se habria truncado (en cuyo caso
 *   'out' no debe usarse).
 */
int jbs_config_build_path(char *out, size_t out_size, const char *filename);

/*
 * jbs_config_filename_is_safe: true si 'filename' es un nombre de archivo
 * simple y seguro para vivir directamente dentro de la carpeta de
 * JheraldBootSound: sin '/', sin '\', sin ':', sin la secuencia "..", no
 * vacio, y por debajo de JBS_AUDIO_FILENAME_MAX bytes (sin contar el
 * terminador). NO comprueba que el archivo exista.
 */
int jbs_config_filename_is_safe(const char *filename);

#endif /* JBS_CONFIG_H */
