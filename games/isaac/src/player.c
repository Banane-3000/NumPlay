/* Isaac: walking, crying, his hearts, and his stats as Rebirth works them out.
 *
 * Damage: 3.5 x sqrt(1 + 1.2 x the damage ups) plus what is added flat, then
 * the items that multiply it. Tears: a delay between tears of 16 - 6 x
 * sqrt(1.3 x the tears ups + 1) frames (10 to begin with, 5 at most), which
 * some items stretch. Range 23.75, shot speed 1 (a tear goes 10 of the game's
 * units a frame) and speed 1 (2 at most). Damage takes soul and black hearts
 * before red ones; a black heart lost hurts every monster in the room. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

Player pl;
const int8_t DIR_X[4] = {0, -1, 0, 1}, DIR_Y[4] = {1, 0, -1, 0};

#define BASE_SPEED 3.2f   /* pixels a frame at speed 1 */

void player_reset(void) {
  memset(&pl, 0, sizeof pl);
  pl.containers = 3, pl.red = 6;
  pl.bombs = 1;
  pl.face = DIR_DOWN, pl.shoot_dir = DIR_NONE;
  pl.x = 160, pl.y = 120;
  player_stats();
}

static float tear_delay_of(float t) {
  float d = t >= 0 ? 16 - 6 * fsqrt(1.3f * t + 1) : 16 - 6 * fsqrt(1.3f * t + 1 > 0 ? 1.3f * t + 1 : 0) - 6 * t;
  return d < 5 ? 5 : d;
}

/* every item's stats, from scratch (as the game's cache) */
void player_stats(void) {
  const uint8_t *h = pl.has;
  float dmg_up = 0, flat = 0, mult = 1, tears = 0, range = 23.75f, shot = 1, speed = 1, luck = 0;
  uint32_t tf = 0;
  /* damage ups */
  dmg_up += h[7] * 1.0f + h[51] * 1.0f + h[50] * 1.0f + h[79] * 1.0f + h[70] * 1.0f + h[90] * 1.0f + h[193] * 0.3f
            + h[138] * 0.3f + h[80] * 0.5f + h[4] * 0.5f + h[12] * 0.3f + h[121] * 1.0f + h[197] * 0.5f + h[201] * 0.3f
            + h[165] * 0.3f + h[101] * 0.3f + h[189] * 0.3f + h[182] * 1.0f + h[122] * 0 + h[154] * 0;
  if (h[4]) mult *= 1.5f;
  if (h[12]) mult *= 1.5f;
  if (h[169]) mult *= 2, flat += 4;
  if (h[182]) mult *= 2.3f;
  if (h[152]) mult *= 0.65f;
  if (h[261]) mult *= 2;
  if (h[120]) mult *= 0.9f;
  if (h[2] || h[153]) mult *= 1;   /* (multi-shot keeps the damage) */
  if (h[122] && pl.red <= 1 && pl.soul == 0) dmg_up += 1.5f, speed += 0.3f;   /* Whore of Babylon at half a heart */
  if (h[109]) flat += pl.coins * 0.04f;                                       /* Money = Power */
  if (h[157]) flat += pl.lust * 0.5f > 3 ? 3 : pl.lust * 0.5f;                /* Bloody Lust */
  /* tears */
  tears += h[1] * 0.7f + h[32] * 0.7f + h[6] * 1.5f + h[72] * 0.5f + h[80] * 0.7f + h[183] * 0.7f + h[120] * 1.7f
           + h[189] * 0.2f + h[101] * 0.2f + h[173] * 0 + h[12] * 0 - h[182] * 0.4f;
  /* range (Rebirth's units, 23.75 to begin with) */
  range += h[29] * 5.25f + h[30] * 5.25f + h[31] * 5.25f + h[14] * 5.25f + h[71] * 2.5f + h[12] * 5.25f + h[121] * 5.25f
           + h[101] * 0.25f + h[189] * 0.25f + h[197] * 0.25f - h[6] * 17.62f + h[5] * 5.25f - h[261] * 10;
  /* shot speed */
  shot += h[143] * 0.2f + h[165] * 0.23f + h[176] * 0.16f + h[183] * 0.16f + h[194] * 0.16f - h[169] * 0 - h[2] * 0;
  /* speed */
  speed += h[13] * 0.2f + h[14] * 0.6f + h[27] * 0.3f + h[28] * 0.3f + h[70] * 0.4f + h[71] * 0.3f + h[12] * 0.3f
           + h[79] * 0.2f + h[82] * 0.3f + h[101] * 0.3f + h[189] * 0.2f + h[143] * 0.3f + h[119] * 0.3f + h[120] * 0.3f
           - h[121] * 0.1f - h[90] * 0.2f - h[129] * 0.2f;
  if (pl.unicorn) speed += 0.28f;
  luck += h[46] * 1.0f + h[189] * 1.0f + h[101] * 0;
  tears += pl.pill_tears, range += pl.pill_range, speed += pl.pill_speed, luck += pl.pill_luck;
  /* the damage */
  float d = 3.5f * fsqrt(1 + 1.2f * dmg_up) + flat;
  if (pl.belial) d += 2;
  d += pl.room_damage;
  d *= mult;
  if (h[245]) d *= 1;   /* 20/20: two tears at full damage (Rebirth) */
  pl.damage = d < 0.5f ? 0.5f : d;
  pl.tears_ups = tears, pl.damage_ups = dmg_up;
  float delay = tear_delay_of(tears);
  if (h[2]) delay = delay * 2.1f + 3;                 /* The Inner Eye */
  if (h[153]) delay = delay * 2.1f + 3;               /* Mutant Spider */
  if (h[169]) delay = delay * 2.1f + 3;               /* Polyphemus */
  if (h[52]) delay = delay * 2.5f;                    /* Dr. Fetus */
  if (h[149]) delay = delay * 1.33f + 10;             /* Ipecac */
  if (h[118]) delay = delay * 3;                      /* Brimstone: charging */
  if (h[69]) delay = delay * 2.5f;                    /* Chocolate Milk */
  if (h[152]) delay = 2;                              /* Technology 2: a continuous laser (shots of it) */
  if (h[6]) delay = tear_delay_of(tears) < 5 ? 5 : delay;
  pl.tear_delay = delay;
  if (range < 6) range = 6;
  pl.range = range;
  if (shot < 0.6f) shot = 0.6f;
  pl.shot_speed = shot;
  if (speed > 2) speed = 2;
  if (speed < 0.1f) speed = 0.1f;
  pl.speed = speed;
  pl.luck = luck;
  /* what tears do */
  if (h[3] || pl.telepathy || h[182]) tf |= TF_HOMING;
  if (h[48]) tf |= TF_PIERCE;
  if (h[115] || h[185] || h[159]) tf |= TF_SPECTRAL;
  if (h[103]) tf |= TF_POISON;
  if (h[89]) tf |= TF_SLOW;
  if (h[110]) tf |= TF_FREEZE;
  if (h[104]) tf |= TF_SPLIT;
  if (h[149]) tf |= TF_EXPLODE | TF_ARC;
  if (h[5]) tf |= TF_BOOMERANG;
  if (h[150]) tf |= TF_TOOTH;
  if (h[132]) tf |= TF_COAL;
  if (h[201]) tf |= TF_CONFUSE;
  if (h[261]) tf |= TF_PROPTOSIS;
  if (h[169] || h[261]) tf |= TF_BIG;
  if (h[7] || h[51] || h[79] || h[80] || h[118] || h[157] || h[193] || h[186] || pl.belial || pl.damage >= 9) tf |= TF_BLOOD;
  pl.tear_flags = tf;
  pl.shots = h[153] ? 4 : h[2] ? 3 : h[245] ? 2 : 1;
  pl.flight = h[20] || h[82] || h[159] || h[179] || h[184] || h[185] || pl.bible || pl.pony;
}

int player_hearts_total(void) { return pl.red + pl.soul + pl.eternal; }

void player_heal(int halves) {
  int max = pl.containers * 2;
  pl.red = (uint8_t)(pl.red + halves > max ? max : pl.red + halves);
}

void player_add_soul(int halves, bool black) {
  int room = 24 - pl.containers * 2 - pl.soul;   /* twelve hearts in all */
  if (room <= 0) return;
  if (halves > room) halves = room;
  for (int i = 0; i < halves; i++) {
    if (black) pl.black |= (uint16_t)(1 << (pl.soul / 2));
    pl.soul++;
  }
}

void player_add_containers(int n) {
  int c = pl.containers + n;
  if (c > 12) c = 12;
  if (c < 0) c = 0;
  pl.containers = (uint8_t)c;
  if (pl.containers * 2 + pl.soul > 24) pl.soul = (uint8_t)(24 - pl.containers * 2);
  if (pl.red > pl.containers * 2) pl.red = (uint8_t)(pl.containers * 2);
}

void player_kill(int source) {
  g_killer = source;
  if (pl.lives) {   /* 1up!, Dead Cat: back up, at the room's start */
    pl.lives--;
    if (pl.has[81] && !pl.has[11]) {
      pl.containers = 1, pl.red = 2, pl.soul = 0, pl.black = 0;
    } else {
      pl.red = (uint8_t)(pl.containers * 2);
      if (!pl.containers) pl.soul = 6;
    }
    pl.invuln = 60;
    fx_text(pl.x, pl.y - 30, "1UP");
    return;
  }
  pl.dead = true;
  pl.hurt_anim = 50;
}

bool player_hurt(int halves, int source) {
  if (pl.invuln > 0 || pl.dead || pl.shadows > 0 || pl.unicorn > 0) return false;
  if (pl.has[108]) halves = 1;   /* The Wafer */
  g_shake = 6;
  pl.hurt_anim = 12;
  pl.invuln = pl.has[162] ? 45 : 30;
  if (pl.has[156] && pl.active) {   /* Habit: a hit charges the active item */
    const ItemInfo *ii = item_info(pl.active);
    if (ii && pl.active_charge < ii->charge) pl.active_charge++;
  }
  if (pl.has[157]) pl.lust++, player_stats();
  for (int i = 0; i < halves; i++) {
    if (pl.eternal) {
      pl.eternal = 0;
      continue;
    }
    if (pl.soul) {
      int idx = (pl.soul - 1) / 2;
      bool black = pl.black & (1 << idx);
      pl.soul--;
      if (pl.soul % 2 == 0 && black) {   /* a whole black heart: the Necronomicon's blast */
        pl.black &= (uint16_t)~(1 << idx);
        for (int k = 0; k < MAX_ENTS; k++)
          if (ents[k].kind == EK_MON && !(ents[k].flags & EF_FRIEND)) ent_damage(&ents[k], 40, 0, 0, 0);
      }
      continue;
    }
    if (pl.red) {
      pl.red--;
      pl.damage_taken_floor = 1;
    }
  }
  if (pl.has[142] && pl.red + pl.soul <= 1) player_add_soul(2, false), pl.has[142] = 2;   /* Scapular */
  if (pl.red == 0 && pl.soul == 0 && pl.eternal == 0) player_kill(source);
  if (pl.has[122]) player_stats();
  return true;
}

/* ---------------------------------------------------------------- crying */
static void cry(int dir) {
  float sp = pl.shot_speed * 5;
  float vx = DIR_X[dir] * sp, vy = DIR_Y[dir] * sp;
  /* (some of his own movement goes into the tear, as in the game) */
  vx += pl.vx * 0.25f, vy += pl.vy * 0.25f;
  float hx = pl.x, hy = pl.y;
  uint32_t flags = pl.tear_flags;
  float dmg = pl.damage;
  /* Chocolate Milk: as charged */
  if (pl.has[69]) {
    float k = pl.charge / (pl.tear_delay * 1.0f);
    if (k > 1) k = 1;
    dmg *= 0.25f + 1.75f * k;
  }
  if (pl.has[52]) {   /* Dr. Fetus: bombs instead */
    Ent *b = ent_new(EK_BOMB, hx + DIR_X[dir] * 8, hy + DIR_Y[dir] * 6);
    if (b) b->vx = vx * 0.9f, b->vy = vy * 0.9f, b->t = 30, b->type = 3, b->hp = dmg * 5 + 30, b->spr = SPR_BOMB;
    return;
  }
  if (pl.has[68] || pl.has[152]) {   /* Technology: a laser */
    laser_fire(hx, hy - 10, dir, 3, pl.has[152] ? dmg * 0.35f : dmg, pl.has[152] ? 2 : 3, 1, false);
    if (!pl.has[152]) return;
  }
  int n = pl.shots;
  /* Loki's Horns and Mom's Eye: by luck, more ways */
  bool loki = pl.has[87] && rnd(100) < 25 + (int)(pl.luck * 5);
  bool back = pl.has[55] && rnd(100) < 50 + (int)(pl.luck * 10);
  for (int k = 0; k < n; k++) {
    float ox = 0, oy = 0, ax = 0, ay = 0;
    if (n > 1) {
      float s = k - (n - 1) / 2.0f;
      if (pl.has[245] && n == 2) {   /* 20/20: side by side */
        ox = DIR_Y[dir] * s * 6, oy = DIR_X[dir] * s * 6;
      } else {                        /* a fan */
        ax = DIR_Y[dir] * s * 0.9f * (pl.has[153] ? 0.6f : 1), ay = -DIR_X[dir] * s * 0.9f * (pl.has[153] ? 0.6f : 1);
        ax = -ax, ay = -ay;
      }
    } else {
      ox = DIR_Y[dir] * (pl.tear_eye ? 3 : -3), oy = DIR_X[dir] * (pl.tear_eye ? 3 : -3);
    }
    Tear *t = tear_fire(hx + ox, hy + oy, vx + ax, vy + ay, dmg, flags);
    (void)t;
  }
  if (loki)
    for (int d = 0; d < 4; d++)
      if (d != dir) tear_fire(hx, hy, DIR_X[d] * sp, DIR_Y[d] * sp, dmg, flags);
  if (back && !loki) tear_fire(hx, hy, -DIR_X[dir] * sp, -DIR_Y[dir] * sp, dmg, flags);
  pl.tear_eye ^= 1;
}

/* Mom's Knife: held in front of him; charged, it flies out and comes back */
static Ent *knife;
static void knife_update(int dir, bool held) {
  if (!knife || knife->kind != EK_FX || knife->type != FX_SPARK + 100) {
    knife = ent_new(EK_FX, pl.x, pl.y);
    if (!knife) return;
    knife->type = FX_SPARK + 100, knife->t = 32767, knife->spr = SPR_KNIFE, knife->state = 0;
  }
  Ent *k = knife;
  k->t = 32767;
  if (k->state == 0) {   /* held: charging while he cries */
    if (dir != DIR_NONE) k->a = (float)dir;
    int d = (int)k->a;
    if (held) pl.charge = pl.charge < 40 ? pl.charge + 1 : 40;
    else if (pl.charge > 4) {
      k->state = 1, k->b = 8 + pl.charge * 0.35f, k->c = 0, k->hp = pl.charge;
      pl.charge = 0;
    } else
      pl.charge = 0;
    k->x = pl.x + DIR_X[d] * 12, k->y = pl.y + DIR_Y[d] * 10 - 6;
  } else {   /* thrown: out, then back */
    int d = (int)k->a;
    k->c += k->b;
    k->b -= 1.1f;
    if (k->c <= 0 && k->b < 0) k->state = 0, k->c = 0;
    k->x = pl.x + DIR_X[d] * (12 + k->c), k->y = pl.y + DIR_Y[d] * (10 + k->c) - 6;
  }
  /* it cuts whatever it touches, every other frame */
  if (g_frame & 1) {
    float dmg = pl.damage * (k->state ? 2 + k->hp / 10.0f : 2);
    for (int i = 0; i < MAX_ENTS; i++) {
      Ent *e = &ents[i];
      if (e->kind != EK_MON || (e->flags & (EF_FRIEND | EF_HIDDEN))) continue;
      float dx = e->x - k->x, dy = e->y - (k->y + 6);
      if (dx * dx + dy * dy < (e->r + 7) * (e->r + 7)) ent_damage(e, dmg, 0, DIR_X[(int)k->a] * 2.0f, DIR_Y[(int)k->a] * 2.0f);
    }
  }
}

static int last_shoot_key = DIR_NONE;
static int shoot_dir_from(uint32_t keys) {
  static const uint32_t kk[4] = {K_SDOWN, K_SLEFT, K_SUP, K_SRIGHT};
  /* the key pressed last wins, as the game does with two at once */
  for (int d = 0; d < 4; d++)
    if ((g_pressed & kk[d])) last_shoot_key = d;
  if (last_shoot_key != DIR_NONE && (keys & kk[last_shoot_key])) return last_shoot_key;
  for (int d = 0; d < 4; d++)
    if (keys & kk[d]) return d;
  return DIR_NONE;
}

void player_update(uint32_t keys) {
  if (pl.dead) {
    if (pl.hurt_anim > 0) pl.hurt_anim--;
    return;
  }
  if (pl.invuln > 0) pl.invuln--;
  if (pl.hurt_anim > 0) pl.hurt_anim--;
  if (pl.hold_anim > 0) pl.hold_anim--;
  /* walking: towards the speed the arrows ask for, with the game's slide */
  float mx = 0, my = 0;
  if (keys & K_LEFT) mx -= 1;
  if (keys & K_RIGHT) mx += 1;
  if (keys & K_UP) my -= 1;
  if (keys & K_DOWN) my += 1;
  if (mx && my) mx *= 0.7071f, my *= 0.7071f;
  float max = BASE_SPEED * pl.speed;
  {   /* cobwebs hold him back */
    int c = (int)((pl.x - ROOM_X) / TILE), r = (int)((pl.y - ROOM_Y) / TILE);
    if (c >= 0 && r >= 0 && c < COLS && r < ROWS && room_cur()->grid[r][c] == CELL_COBWEB && !pl.flight) max *= 0.3f;
  }
  float tx = mx * max, ty = my * max;
  float acc = (mx || my) ? 0.35f : 0.25f;
  pl.vx += (tx - pl.vx) * acc, pl.vy += (ty - pl.vy) * acc;
  if (absf(pl.vx) < 0.05f && !mx) pl.vx = 0;
  if (absf(pl.vy) < 0.05f && !my) pl.vy = 0;
  if (pl.pony > 0 && (mx || my)) pl.vx = mx * 7, pl.vy = my * 7;   /* (A Pony's dash) */
  move_ent(&pl.x, &pl.y, &pl.vx, &pl.vy, 5, pl.flight, false);
  if (mx || my) {
    pl.walk++;
    if (absf(mx) > absf(my)) pl.face = mx < 0 ? DIR_LEFT : DIR_RIGHT;
    else pl.face = my < 0 ? DIR_UP : DIR_DOWN;
  } else
    pl.walk = 0;
  /* crying */
  int dir = shoot_dir_from(keys);
  pl.shoot_dir = (int8_t)dir;
  if (pl.fire_cd > 0) pl.fire_cd -= 1;
  if (pl.has[114]) {
    knife_update(dir, dir != DIR_NONE);
  } else if (pl.has[118]) {   /* Brimstone: charge, then let go */
    static int bdir = DIR_DOWN;
    if (dir != DIR_NONE) {
      bdir = dir;
      if (pl.charge < pl.tear_delay) pl.charge++;
    } else if (pl.charge >= pl.tear_delay && pl.charge > 0) {
      laser_fire(pl.x, pl.y - 10, bdir, 9, pl.damage * 0.33f, 15, 0, false);
      pl.charge = 0;
    } else
      pl.charge = 0;
  } else if (pl.has[69]) {     /* Chocolate Milk: hold to charge */
    static int cdir = DIR_DOWN;
    if (dir != DIR_NONE) {
      cdir = dir;
      if (pl.charge < pl.tear_delay) pl.charge++;
    } else if (pl.charge > 0) {
      cry(cdir);
      pl.charge = 0;
    }
  } else if (dir != DIR_NONE && pl.fire_cd <= 0) {
    cry(dir);
    pl.fire_cd += pl.tear_delay + 1;
    if (pl.fire_cd < 1) pl.fire_cd = 1;
  }
  if (dir == DIR_NONE && pl.fire_cd < 0) pl.fire_cd = 0;
  /* spikes (not flying) */
  if (!pl.flight && grid_spikes_at(pl.x, pl.y)) {
    if (room_cur()->kind == ROOM_SACRIFICE) {
      if (player_hurt(1, -2)) {
        Room *r = room_cur();
        r->variant++;
        /* the sacrifice room's rewards, as in Rebirth */
        int n = r->variant;
        if (n <= 2 && rnd(2) == 0) pickup_spawn(PK_PENNY, 0, 160, 140, true);
        else if (n == 3 || n == 4) pickup_spawn(PK_CHEST, 0, 160, 145, true);
        else if (n == 6) pedestal_spawn(item_from_pool(POOL_ANGEL), 160, 150, 0, false);
        else if (n >= 8 && n <= 11 && rnd(3) == 0) pickup_spawn(PK_SOUL, 0, 160, 140, true);
        else if (n == 12) floor_enter_room(fl.boss_room, DIR_NONE);
      }
    } else
      player_hurt(1, -2);
  }
  if (pl.creep > 0) pl.creep--;
  if (pl.belial > 0) pl.belial--;
}

/* ---------------------------------------------------------------- drawing */
void player_draw_shadow(void) {
  int hover = pl.flight ? 3 : 0;
  gfx_ellipse((int)pl.x, (int)pl.y, 7 - hover / 2, 3, C_BLACK, 12);
}

void player_draw(void) {
  int x = (int)pl.x, y = (int)pl.y;
  if (pl.flight) y -= 4 + (int)((g_frame / 6) % 2);
  bool flash = pl.invuln > 0 && (pl.invuln / 3) % 2 == 0;
  int alpha = 32;
  uint16_t tint = C_RED;
  int amount = flash ? 14 : 0;
  if (pl.dead) {   /* fallen over, crying */
    gfx_sprite_tint(SPR_ISAAC_HEAD, 1, x, y + 2, 0, tint, 0, alpha);
    return;
  }
  if (pl.hold_anim > 0) {   /* holding up what he just found */
    gfx_sprite(SPR_ISAAC_BODY, 8, x, y, 0, alpha);
    gfx_sprite(SPR_ISAAC_HEAD, 0, x, y - 7, 0, alpha);
    if (pl.hold_item > 0) gfx_sprite(item_sprite(pl.hold_item), 0, x, y - 22, 0, 32);
    return;
  }
  int body, flags = 0;
  int step = (pl.walk / 4) % 4;
  int f = pl.face;
  if (f == DIR_LEFT || f == DIR_RIGHT) body = 4 + (pl.walk ? step : 0), flags = f == DIR_LEFT ? GFX_FLIPX : 0;
  else body = pl.walk ? step : 0;
  int bob = (pl.walk && (step & 1)) ? 1 : 0;
  gfx_sprite_tint(SPR_ISAAC_BODY, body, x, y, flags, tint, amount, alpha);
  /* the head looks the way he cries, or else the way he walks */
  int hd = pl.shoot_dir != DIR_NONE ? pl.shoot_dir : pl.face;
  bool crying = pl.shoot_dir != DIR_NONE && (pl.fire_cd > pl.tear_delay * 0.5f || pl.has[118] || pl.has[69]);
  int head = hd == DIR_UP ? 4 : (hd == DIR_LEFT || hd == DIR_RIGHT) ? 2 : 0;
  if (crying) head++;
  int hflags = hd == DIR_LEFT ? GFX_FLIPX : 0;
  gfx_sprite_tint(SPR_ISAAC_HEAD, head, x, y - 7 + bob, hflags, tint, amount, alpha);
  /* Brimstone and Chocolate Milk charge in a bar over his head */
  if ((pl.has[118] || pl.has[69] || pl.has[114]) && pl.charge > 0) {
    float max = pl.has[114] ? 40 : pl.tear_delay;
    int w = (int)(14 * pl.charge / max);
    gfx_rect(x - 8, y - 34, 16, 4, C_BLACK, 24);
    gfx_rect(x - 7, y - 33, w, 2, pl.charge >= max ? rgb565(0xFFE060) : rgb565(0xE04040), 32);
  }
}
