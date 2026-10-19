/* Plays The Binding of Isaac on a computer, with scripted keys, and saves frames.
 *
 *   play [--frames N] [--keys "10-40:right,50:ok,..."] [--shots 30,60|all|every:N] [--out DIR] [--saves DIR]
 *        [--seed N] [--run] [--stage S] [--give ID,ID...] [--bot] [--god] [--boss MON] [--goto KIND]
 *
 * Keys: left right up down sleft sright sup sdown ok back bomb item card map home; "a-b:key" holds the key
 * from frame a to b, "a:key" for one frame. --run starts a new run past the title (--stage: on that floor).
 * --bot plays by itself: it goes for the monsters, crying at them, then on through the doors to the rooms it
 * hasn't seen, then down the trapdoor; --god keeps Isaac alive; --goto starts in the floor's first room of that kind
 * (ROOM_* in src/data.h), --boss makes the floor's boss that one (MON_*). Screenshots are DIR/shot_<frame>.ppm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/game.h"
#include "../src/gfx.h"
#include "../src/res.h"

extern uint32_t host_keys, host_time;
void host_shot(const char *path);
void host_save_dir(const char *d);
extern const uint8_t is_data[];
int floor_neighbour(int d);

typedef struct { int a, b; uint32_t k; } Hold;
static Hold holds[4096];
static int nholds;

static uint32_t key_of(const char *s) {
  static const struct { const char *n; uint32_t k; } names[] = {
    {"left", K_LEFT}, {"right", K_RIGHT}, {"up", K_UP}, {"down", K_DOWN}, {"sleft", K_SLEFT}, {"sright", K_SRIGHT},
    {"sup", K_SUP}, {"sdown", K_SDOWN}, {"ok", K_OK}, {"back", K_BACK}, {"bomb", K_BOMB}, {"item", K_ITEM},
    {"card", K_CARD}, {"map", K_MAP}, {"home", K_HOME}};
  for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
    if (!strcmp(s, names[i].n)) return names[i].k;
  fprintf(stderr, "unknown key %s\n", s);
  exit(2);
}

static void parse_keys(char *s) {
  for (char *tok = strtok(s, ","); tok; tok = strtok(NULL, ",")) {
    char *colon = strchr(tok, ':');
    if (!colon) continue;
    *colon = 0;
    int a, b;
    if (sscanf(tok, "%d-%d", &a, &b) != 2) b = a = atoi(tok);
    holds[nholds++] = (Hold){a, b, key_of(colon + 1)};
  }
}

/* ---------------------------------------------------------------- the bot */
static int bot_stuck, bot_last_room = -1, bot_target_door = -1, bot_target = -1, bot_chase, bot_ignore_room = -1;
static uint8_t bot_ignore[MAX_ENTS];
static float bot_lx, bot_ly;
static uint32_t bot_keys(void) {
  uint32_t k = 0;
  if (g_mode == MODE_WIN || g_mode == MODE_STAGE) return (g_frame & 8) ? K_OK : 0;
  if (g_mode != MODE_PLAY || g_transition) return 0;
  Ent *m = NULL;
  float best = 1e9f;
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (e->kind != EK_MON || (e->flags & (EF_FRIEND | EF_HIDDEN | EF_NOCOUNT))) continue;
    float d = dist2(e->x, e->y, pl.x, pl.y);
    if (d < best) best = d, m = e;
  }
  float tx = 160, ty = 120;
  if (m) {
    /* line up with it, a little way off, and cry at it */
    float dx = m->x - pl.x, dy = m->y - pl.y;
    static const float off[4] = {55, 30, -55, -30};
    if (absf(dx) < absf(dy)) {
      tx = m->x, ty = m->y + (dy > 0 ? -50 : 50);
      for (int i = 0; i < 4 && solid_at(tx, ty, false); i++) ty = m->y + (dy > 0 ? -off[i] : off[i]);
      if (absf(dx) < 12) k |= dy > 0 ? K_SDOWN : K_SUP;
    } else {
      ty = m->y, tx = m->x + (dx > 0 ? -55 : 55);
      for (int i = 0; i < 4 && solid_at(tx, ty, false); i++) tx = m->x + (dx > 0 ? -off[i] : off[i]);
      if (absf(dy) < 12) k |= dx > 0 ? K_SRIGHT : K_SLEFT;
    }
    if (!(k & K_SHOOT)) k |= absf(dx) > absf(dy) ? (dx > 0 ? K_SRIGHT : K_SLEFT) : (dy > 0 ? K_SDOWN : K_SUP);
  } else {
    /* pickups and items first, then the trapdoor, then a door to a room not yet visited */
    Ent *p = NULL;
    best = 1e9f;
    if (bot_ignore_room != fl.cur) memset(bot_ignore, 0, sizeof bot_ignore), bot_ignore_room = fl.cur;
    for (int i = 0; i < MAX_ENTS; i++) {
      Ent *e = &ents[i];
      if (bot_ignore[i]) continue;
      bool want = (e->kind == EK_PEDESTAL && e->t3 && !(e->flags & EF_DEVIL) && !e->price) || e->kind == EK_TRAPDOOR
                  || (e->kind == EK_PICKUP && !(e->flags & EF_SHOP) && e->type != PK_LOCKED_CHEST);
      if (!want) continue;
      float d = dist2(e->x, e->y, pl.x, pl.y) + (e->kind == EK_TRAPDOOR ? 1e6f : 0);
      if (d < best) best = d, p = e;
    }
    /* (what it can't reach after a while, it leaves) */
    if (p && (int)(p - ents) == bot_target) {
      if (++bot_chase > 150 && p->kind != EK_TRAPDOOR) bot_ignore[p - ents] = 1, bot_chase = 0;
    } else
      bot_target = p ? (int)(p - ents) : -1, bot_chase = 0;
    if (p && !(p->kind == EK_TRAPDOOR && bot_target_door >= 0 && fl.cur != fl.boss_room)) tx = p->x, ty = p->y;
    else {
      if (fl.cur != bot_last_room) bot_last_room = fl.cur, bot_target_door = -1;
      if (bot_target_door < 0) {
        int unvisited = -1, any = -1;
        for (int d = 0; d < 4; d++) {
          if (!room_door_exists(d, NULL) || !room_door_open(d)) continue;
          int j = floor_neighbour(d);
          if (j < 0) continue;
          if (any < 0 || rand() % 3 == 0) any = d;
          if (!(fl.rooms[j].state & RS_VISITED) && (unvisited < 0 || rand() % 2)) unvisited = d;
        }
        bot_target_door = unvisited >= 0 ? unvisited : any;
      }
      static const float dx[4] = {160, ROOM_X + 2, 160, ROOM_R - 2}, dy[4] = {ROOM_B - 2, 120, ROOM_Y + 2, 120};
      if (bot_target_door >= 0) tx = dx[bot_target_door], ty = dy[bot_target_door];
    }
  }
  float px, py;
  path_dir(pl.x, pl.y, tx, ty, pl.flight, &px, &py);
  if (dist2(tx, ty, pl.x, pl.y) > 9) {
    if (px < -0.3f) k |= K_LEFT;
    if (px > 0.3f) k |= K_RIGHT;
    if (py < -0.3f) k |= K_UP;
    if (py > 0.3f) k |= K_DOWN;
  }
  if (bot_target_door >= 0 && !m && dist2(tx, ty, pl.x, pl.y) < 20 * 20) {
    static const uint32_t way[4] = {K_DOWN, K_LEFT, K_UP, K_RIGHT};
    k |= way[bot_target_door];
  }
  /* stuck: a random way for a while; a bomb now and then */
  if (dist2(pl.x, pl.y, bot_lx, bot_ly) < 0.5f && (k & K_MOVE)) bot_stuck++;
  else bot_stuck = 0;
  bot_lx = pl.x, bot_ly = pl.y;
  if (bot_stuck > 40) {
    k = (k & ~K_MOVE) | (uint32_t[]){K_LEFT, K_RIGHT, K_UP, K_DOWN}[(g_frame / 20) % 4];
    if (bot_stuck > 200) bot_target_door = -1, bot_stuck = 0;
  }
  if (rand() % 400 == 0 && pl.bombs > 3) k |= K_BOMB;
  if (rand() % 200 == 0) k |= K_ITEM;
  if (rand() % 300 == 0) k |= K_CARD;
  return k;
}

int main(int argc, char **argv) {
  int frames = 600, seed = 1, stage = 1, every = 0;
  char keys[65536] = "", shots[65536] = "", out[512] = "build/shots", saves[512] = "build/host-saves", give[1024] = "";
  bool all = false, run = false, bot = false, god = false;
  int boss = -1, gokind = -1;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const char *v = i + 1 < argc ? argv[i + 1] : "";
    if (!strcmp(a, "--frames")) frames = atoi(v), i++;
    else if (!strcmp(a, "--keys")) snprintf(keys, sizeof keys, "%s", v), i++;
    else if (!strcmp(a, "--shots")) {
      if (!strcmp(v, "all")) all = true;
      else if (!strncmp(v, "every:", 6)) every = atoi(v + 6);
      else snprintf(shots, sizeof shots, ",%s,", v);
      i++;
    } else if (!strcmp(a, "--out")) snprintf(out, sizeof out, "%s", v), i++;
    else if (!strcmp(a, "--saves")) snprintf(saves, sizeof saves, "%s", v), i++;
    else if (!strcmp(a, "--seed")) seed = atoi(v), i++;
    else if (!strcmp(a, "--stage")) stage = atoi(v), i++;
    else if (!strcmp(a, "--give")) snprintf(give, sizeof give, "%s", v), i++;
    else if (!strcmp(a, "--boss")) boss = atoi(v), i++;
    else if (!strcmp(a, "--goto")) gokind = atoi(v), i++;
    else if (!strcmp(a, "--run")) run = true;
    else if (!strcmp(a, "--bot")) bot = true;
    else if (!strcmp(a, "--god")) god = true;
  }
  mkdir(out, 0755);
  mkdir(saves, 0755);
  host_save_dir(saves);
  parse_keys(keys);
  srand((unsigned)seed);
  is_bin = is_data;
  g_rng = 0x9E3779B9u * (uint32_t)seed;
  game_init();
  if (run) {
    host_time = (uint32_t)seed * 7919u;
    game_new_run();
    if (stage > 1) {
      while (fl.stage < stage) game_next_floor();
    }
    if (boss >= 0) fl.boss = (uint8_t)boss, fl.rooms[fl.boss_room].boss = (uint8_t)boss;
    for (char *tok = strtok(give, ","); tok; tok = strtok(NULL, ",")) item_give(atoi(tok));
    g_mode = MODE_PLAY;
    if (gokind == ROOM_DEVIL || gokind == ROOM_ANGEL) {   /* (straight into the Devil's or an angel's room) */
      fl.devil_angel = gokind == ROOM_ANGEL;
      floor_open_devil();
      room_teleport(fl.devil_room);
    } else if (gokind >= 0)
      for (int i = 0; i < fl.nrooms; i++)
        if (fl.rooms[i].kind == gokind) {
          room_teleport(i);
          break;
        }
  }
  int floors_seen = fl.stage;
  for (int f = 0; f < frames && g_running; f++) {
    uint32_t k = 0;
    for (int i = 0; i < nholds; i++)
      if (f >= holds[i].a && f <= holds[i].b) k |= holds[i].k;
    if (bot && !k) k = bot_keys();
    if (getenv("TRACE") && f % 50 == 0) printf("f %d pl %.1f,%.1f v %.2f,%.2f keys %x mode %d room %d\n", f, pl.x, pl.y, pl.vx, pl.vy, k, g_mode, fl.cur);
    if (god) pl.invuln = pl.invuln > 2 ? pl.invuln : 2, pl.dead = false;
    host_keys = k;
    host_time += 33;
    game_frame(k);
    if (fl.stage > floors_seen) floors_seen = fl.stage;
    game_draw();
    char tag[32];
    snprintf(tag, sizeof tag, ",%d,", f);
    if (all || (every && f % every == 0) || strstr(shots, tag)) {
      char p[600];
      snprintf(p, sizeof p, "%s/shot_%05d.ppm", out, f);
      host_shot(p);
    }
  }
  for (int i = 0; i < MAX_ENTS; i++)
    if (ents[i].kind == EK_MON)
      printf("MON %d type %d hp %.1f/%.1f state %d at %.0f,%.0f flags %x\n", i, ents[i].type, ents[i].hp, ents[i].maxhp,
             ents[i].state, ents[i].x, ents[i].y, ents[i].flags);
  printf("END mode %d stage %d room %d kind %d hp %d/%d soul %d coins %d bombs %d keys %d items %d frames %u floors %d dropped %d\n",
         g_mode, fl.stage, fl.cur, room_cur()->kind, pl.red, pl.containers * 2, pl.soul, pl.coins, pl.bombs, pl.keys, pl.norder,
         g_frame, floors_seen, gfx_dropped);
  return 0;
}
