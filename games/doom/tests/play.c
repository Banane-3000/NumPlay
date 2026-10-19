/* Plays Doom on a computer, with scripted keys, and saves screenshots.
 *
 *   play [--ms N] [--keys "1000-1500:up,2000:ok,..."] [--shots 3000,6000] [--every MS] [--out DIR]
 *        [--saves DIR] [--stats]
 *
 * Keys are the calculator's (tools/emu.py's names: left up down right ok back home onoff shift alpha xnt
 * var toolbox backspace sqrt 0-9 plus minus mul div exe). "a-b:key" holds it from a ms to b ms, "a:key"
 * for 100 ms. Time goes by as the game sleeps and 1/35 s with each frame drawn. At --ms the script
 * presses Home: the game keeps the moment and quits. Screenshots are DIR/shot_<ms>.ppm; --stats prints
 * what the engine used (zone, columns) at the end. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "doomtype.h"
#include "z_zone.h"
#include "nw.h"
#include "plat.h"

extern uint32_t host_time;
extern uint64_t host_keys;
void host_shot(const char *path);
void host_save_dir(const char *d);
int nw_main(void);

static const struct { const char *name; int key; } names[] = {
    {"left", 0}, {"up", 1}, {"down", 2}, {"right", 3}, {"ok", 4}, {"back", 5}, {"home", 6}, {"onoff", 8},
    {"shift", 12}, {"alpha", 13}, {"xnt", 14}, {"var", 15}, {"toolbox", 16}, {"backspace", 17}, {"sqrt", 28},
    {"7", 30}, {"8", 31}, {"9", 32}, {"4", 36}, {"5", 37}, {"6", 38}, {"mul", 39}, {"div", 40}, {"1", 42},
    {"2", 43}, {"3", 44}, {"plus", 45}, {"minus", 46}, {"0", 48}, {"dot", 49}, {"exe", 52},
};

typedef struct { uint32_t a, b; int key; } Hold;
static Hold holds[4096];
static int nholds;
static uint32_t shots[1024];
static int nshots, next_shot;
static uint32_t every, next_every, end_ms = 10000;
static char out[256] = "build/shots";

static int key_named(const char *s, int n) {
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        if ((int)strlen(names[i].name) == n && !strncmp(names[i].name, s, n)) return names[i].key;
    fprintf(stderr, "play: no key %.*s\n", n, s);
    exit(2);
}

static void parse_keys(const char *s) {
    while (*s) {
        char *e;
        uint32_t a = (uint32_t)strtoul(s, &e, 10), b = a + 100;
        if (*e == '-') b = (uint32_t)strtoul(e + 1, &e, 10);
        if (*e != ':') {
            fprintf(stderr, "play: bad keys at %s\n", s);
            exit(2);
        }
        const char *k = e + 1, *end = k;
        while (*end && *end != ',') end++;
        holds[nholds++] = (Hold){a, b, key_named(k, (int)(end - k))};
        s = *end ? end + 1 : end;
    }
}

static void shot(uint32_t ms) {
    char p[512];
    snprintf(p, sizeof p, "%s/shot_%06u.ppm", out, ms);
    host_shot(p);
}

/* the script, as time goes by (plat_host.c calls it) */
void host_tick(void) {
    uint64_t k = 0;
    for (int i = 0; i < nholds; i++)
        if (host_time >= holds[i].a && host_time < holds[i].b) k |= (uint64_t)1 << holds[i].key;
    if (host_time >= end_ms && host_time < end_ms + 400) k |= (uint64_t)1 << 6; /* Home */
    host_keys = k;
}

/* each frame drawn: 1/35 s goes by, and screenshots are taken */
void host_frame(void) {
    while (next_shot < nshots && host_time >= shots[next_shot]) shot(shots[next_shot++]);
    if (every && host_time >= next_every) {
        shot(host_time);
        next_every = host_time + every;
    }
    host_time += 1000 / 35;
    if (host_time > end_ms + 60000) {
        fprintf(stderr, "play: the game did not quit on Home\n");
        exit(3);
    }
}

int main(int argc, char **argv) {
    const char *saves = "build/host-saves";
    int stats = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ms") && i + 1 < argc) end_ms = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc) parse_keys(argv[++i]);
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) {
            for (char *s = argv[++i]; *s;) {
                shots[nshots++] = (uint32_t)strtoul(s, &s, 10);
                if (*s == ',') s++;
            }
        } else if (!strcmp(argv[i], "--every") && i + 1 < argc) every = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) snprintf(out, sizeof out, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--saves") && i + 1 < argc) saves = argv[++i];
        else if (!strcmp(argv[i], "--stats")) stats = 1;
        else {
            fprintf(stderr, "play: unknown option %s\n", argv[i]);
            return 2;
        }
    }
    mkdir(out, 0755);
    mkdir(saves, 0755);
    host_save_dir(saves);
    int r = nw_main();
    if (stats) printf("STATS zone_free %d\n", Z_FreeMemory());
    printf("END time %u quit %d\n", host_time, (int)nw_quit);
    return r;
}
