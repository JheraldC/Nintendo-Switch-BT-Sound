/*
 * jbs_paths.h  -  rutas fijas usadas por el sysmodule (FASE 2)
 *
 * En Switch, "sdmc:/" lo resuelve el devoptab de libnx tras fsdevMountSdmc().
 * On the SD card this is  SD:/config/JheraldBootSound/  (the drive letter your OS assigns
 * to the card is whatever it is on your system -- there is nothing special about "D:").
 */
#ifndef JBS_PATHS_H
#define JBS_PATHS_H

#define JBS_DIR            "sdmc:/config/JheraldBootSound"

#define JBS_PATH_WAV       JBS_DIR "/startup.wav"         /* default/fallback fijo, siempre este nombre */
#define JBS_PATH_CONFIG    JBS_DIR "/config.ini"           /* FASE 4.0: seleccion de audio_file opcional */
#define JBS_PATH_DEBUGFLAG JBS_DIR "/debug.flag"
#define JBS_PATH_RESULT    JBS_DIR "/phase2_result.txt"   /* FASE 2/2.1 (ya no se escribe en FASE 3+) */
#define JBS_PATH_RESULT3   JBS_DIR "/phase3_result.txt"   /* FASE 3/3.1/3.3 (audout) */
#define JBS_PATH_RESULT4   JBS_DIR "/phase4_result.txt"   /* FASE 4.0 (config.ini + audio custom) */

#define JBS_VERSION_STR    "0.4.0-fase4.0"

#endif /* JBS_PATHS_H */
