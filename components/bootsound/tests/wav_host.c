/*
 * wav_host.c  -  arnes de prueba NATIVO del parser real (source/wav.c + source/sdfile.c)
 *
 * Se ejecuta en PC, SIN Switch. Usa EXACTAMENTE el mismo C que el sysmodule.
 *
 * En esta maquina no hay compilador C de host instalado; cuando lo haya:
 *
 *     gcc -std=c11 -Wall -Wextra -I include \
 *         tests/wav_host.c source/wav.c source/sdfile.c -o tests/wav_host
 *
 *     python tests/gen_fixtures.py
 *     for f in tests/fixtures/*.wav; do ./tests/wav_host "$f"; done
 *
 * Mientras tanto, la logica se valida con el port Python: tests/wav_ref.py +
 * tests/run_tests.py  (mantener ambos sincronizados con source/*.c).
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "wav.h"
#include "sdfile.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "uso: %s <fichero.wav>\n", argv[0]);
        return 2;
    }

    jbs_wav_fmt_t fmt;
    void   *buf   = NULL;
    size_t  alloc = 0;
    uint64_t pcm  = 0;

    jbs_status_t st = jbs_load_startup_pcm(argv[1], &fmt, &buf, &alloc, &pcm);

    printf("%-34s status=%-22s audio_format=%u sr=%u ch=%u bps=%u "
           "block_align=%u byte_rate=%u pcm=%llu alloc=%llu\n",
           argv[1], jbs_status_str(st),
           (unsigned)fmt.audio_format, (unsigned)fmt.sample_rate,
           (unsigned)fmt.num_channels, (unsigned)fmt.bits_per_sample,
           (unsigned)fmt.block_align, (unsigned)fmt.byte_rate,
           (unsigned long long)pcm, (unsigned long long)alloc);

    if (buf) {
        /* comprobar cola a cero */
        const unsigned char *b = (const unsigned char *)buf;
        int tail_ok = 1;
        for (uint64_t i = pcm; i < (uint64_t)alloc; i++) {
            if (b[i] != 0) { tail_ok = 0; break; }
        }
        printf("    tail_zeros_ok=%d  aligned_ok=%d\n",
               tail_ok, ((uintptr_t)buf % 0x1000) == 0);
        free(buf);
    }
    return st == JBS_OK ? 0 : 1;
}
