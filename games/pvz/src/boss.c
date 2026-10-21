/* Dr. Zomboss, in his Zombot, on the roof at night (game.h). His moves follow the game's own: summoning
 * zombies, stomping, bungees, the RV, and lowering his head to spit fire and ice balls, when he can be hurt. */
#include <string.h>
#include "boss_art.h"
#include "game.h"

Boss boss;

/* the Zombot's animations: their first frame and length in its tracks (the .reanim), at 12 frames a second */
const BossClip boss_clips[N_CLIPS] = {
  [CL_ENTER] = {0, 40},    [CL_IDLE] = {40, 15},    [CL_SPAWN5] = {55, 21},  [CL_SPAWN4] = {76, 21},
  [CL_SPAWN3] = {97, 21},  [CL_SPAWN2] = {118, 21}, [CL_SPAWN1] = {139, 22}, [CL_BUNGEE_ENTER] = {161, 25},
  [CL_BUNGEE_LEAVE] = {186, 18}, [CL_STOMP1] = {205, 31}, [CL_STOMP2] = {236, 31}, [CL_STOMP3] = {267, 29},
  [CL_STOMP4] = {296, 29}, [CL_HEAD_ENTER] = {325, 57}, [CL_HEAD_IDLE] = {382, 17}, [CL_ATTACK1] = {399, 40},
  [CL_ATTACK2] = {439, 40}, [CL_ATTACK3] = {479, 40}, [CL_ATTACK4] = {519, 40}, [CL_ATTACK5] = {559, 43},
  [CL_HEAD_LEAVE] = {602, 45}, [CL_DEATH] = {647, 107}, [CL_RV] = {754, 38},
};

static const uint8_t enemies[12] = {Z_CONE, Z_BUCKET, Z_FOOTBALL, Z_POLE, Z_JACK, Z_LADDER, Z_ZOMBONI, Z_CATAPULT,
                                    Z_POGO, Z_NEWSPAPER, Z_DOOR, Z_GARGANTUAR};

static int range(int lo, int hi) { return lo + irand(hi - lo + 1); }

static void enter(int phase, int clip, float duration) {
  if (phase == BS_SPAWN || phase == BS_STOMP || phase == BS_BUNGEE || phase == BS_RV || phase == BS_DEATH) boss.chill = 0;
  boss.phase = (uint8_t)phase, boss.clip = (uint8_t)clip, boss.elapsed = 0;
  boss.fps = phase == BS_RV ? 16 : phase == BS_DEATH || phase == BS_BUNGEE_LEAVE ? 18 : 12;
  boss.duration = duration >= 0 ? duration : (float)((boss_clips[clip].len * 100 + boss.fps - 1) / boss.fps);
}

void boss_start(void) {
  memset(&boss, 0, sizeof boss);
  boss.health = BOSS_HEALTH;
  boss.summon_in = 500, boss.head_in = 5000, boss.stomp_in = -1, boss.bungee_in = -1;
  enter(BS_ENTER, CL_ENTER, -1);
}

bool boss_vulnerable(void) {
  return boss.phase == BS_HEAD_BEFORE || boss.phase == BS_SPIT || boss.phase == BS_HEAD_AFTER;
}

void boss_damage(int amount) {
  if (!boss_vulnerable()) return;
  boss.health = boss.health > amount ? boss.health - amount : 0;
  boss.hit_at = B.ticks;
}

static void crush(int row, int col, int cols) {
  for (int i = 0; i < B.nplants; i++) {
    Plant *p = &B.plants[i];
    if (p->row >= row && p->row <= row + 1 && p->col >= col && p->col < col + cols) p->health = 0;
  }
  fx_add(FX_SMOKE, cell_x(col + cols / 2), row_y(row + 1, 600), 40, 0);
}

static void bungees(int col) {
  for (int i = 0; i < 3; i++) {
    int c = col + i, rows[MAX_ROWS], n = 0;
    for (int k = 0; k < B.nplants; k++)
      if (B.plants[k].col == c && B.plants[k].health > 0 && n < MAX_ROWS) rows[n++] = B.plants[k].row;
    int row = n ? rows[irand(n)] : irand(5);
    Zombie *z = spawn_zombie(Z_BUNGEE, row, cell_x(c), 0);
    if (z) z->row = (uint8_t)row, z->x = cell_x(c), z->phase = BP_DOWN, z->u.bungee.left = 100, z->u.bungee.column = (uint8_t)c,
           z->u.bungee.drop = 0xFF;
  }
}

static bool contact(float before, float frac) {
  return before < boss.duration * frac && boss.elapsed >= boss.duration * frac;
}

void boss_tick(void) {
  /* the fire or ice ball rolling down its lane, flattening what it meets */
  if (boss.ball.on) {
    boss.ball.age++;
    if (boss.ball.age > 100) {
      boss.ball.x -= boss.ball.kind == 0 ? 1036.9f / 56 * 0.02f : 1036.9f / 58 * 0.02f;
      for (int i = 0; i < B.nplants; i++) {
        Plant *p = &B.plants[i];
        float d = cell_x(p->col) - boss.ball.x;
        if (p->row == boss.ball.row && d <= 35 && d >= -35) p->health = 0;
      }
    }
    if (boss.ball.x < -100) boss.ball.on = 0;
  }
  if (boss.health == 0 && boss.phase != BS_DEATH) {
    boss.ball.on = 0, boss.frozen = 0;
    enter(BS_DEATH, CL_DEATH, -1);
    for (int i = 0; i < B.nzombies; i++) B.zombies[i].health = 0, B.zombies[i].flags |= ZF_SPAWNED;
  }
  boss.age++;
  if (boss.chill > 0) boss.chill--;
  if (boss.frozen > 0) {
    boss.frozen--;
    return;
  }
  boss.anim++;
  if (boss.summon_in > 0) boss.summon_in--;
  if (boss.head_in > 0) boss.head_in--;
  if (boss.stomp_in > 0) boss.stomp_in--;
  if (boss.bungee_in > 0) boss.bungee_in--;
  float before = boss.elapsed;
  bool timed = boss.phase == BS_IDLE || boss.phase == BS_HEAD_BEFORE || boss.phase == BS_HEAD_AFTER || boss.phase == BS_BUNGEE_WAIT;
  boss.elapsed += boss.chill > 0 && !timed ? 0.5f : 1;
  if (boss.phase == BS_HEAD_LEAVE && boss.elapsed > boss.duration * 0.23f) boss.chill = 0;
  if (boss.phase == BS_SPAWN && contact(before, 0.6f)) {
    int kind = boss.age < 3500 ? Z_NORMAL : boss.age < 8000 ? Z_CONE : boss.age < 12500 ? Z_BUCKET
             : enemies[irand(boss.row == 0 ? 11 : 12)];
    spawn_zombie(kind, boss.row, 630, 0);
  }
  if (boss.phase == BS_STOMP && contact(before, boss.row >= 2 ? 0.55f : 0.5f)) crush(boss.row, 5, 4);
  if (boss.phase == BS_RV && contact(before, 0.65f)) {
    crush(boss.row, boss.column, 3);
    boss.summon_in = 500, boss.head_in = 5000;
    if (boss.mode >= 1) boss.stomp_in = 4000;
    if (boss.mode >= 2) boss.bungee_in = 6500;
  }
  if (boss.phase == BS_BUNGEE && contact(before, 0.4f)) bungees(boss.column);
  if (boss.phase == BS_SPIT && contact(before, 0.42f)) {
    boss.ball.on = 1, boss.ball.kind = boss.ball_kind, boss.ball.row = boss.row, boss.ball.x = 530, boss.ball.age = 0;
  }
  if (boss.elapsed < boss.duration) return;
  if (boss.phase == BS_DEATH) {
    B.phase = PH_WON;
    B.nzombies = 0, B.nshots = 0;
    B.last_x = 600, B.last_y = row_y(2, 600);
    return;
  }
  if (boss.phase == BS_BUNGEE) enter(BS_BUNGEE_WAIT, CL_BUNGEE_ENTER, 1);
  else if (boss.phase == BS_BUNGEE_WAIT) {
    bool done = true;
    for (int i = 0; i < B.nzombies; i++)
      done &= !(B.zombies[i].kind == Z_BUNGEE && B.zombies[i].phase != BP_UP && B.zombies[i].health > 0);
    if (done) enter(BS_BUNGEE_LEAVE, CL_BUNGEE_LEAVE, -1);
  } else if (boss.phase == BS_HEAD_ENTER) enter(BS_HEAD_BEFORE, CL_HEAD_IDLE, 500);
  else if (boss.phase == BS_HEAD_BEFORE) {
    boss.row = (uint8_t)range(0, 4), boss.ball_kind = (uint8_t)(frand() < 0.5f ? 0 : 1);
    enter(BS_SPIT, CL_ATTACK1 + boss.row, -1);
  } else if (boss.phase == BS_SPIT) enter(BS_HEAD_AFTER, CL_HEAD_IDLE, 300);
  else if (boss.phase == BS_HEAD_AFTER) enter(BS_HEAD_LEAVE, CL_HEAD_LEAVE, -1);
  else if (boss.phase != BS_IDLE) enter(BS_IDLE, CL_IDLE, (float)range(100, 200));
  else {
    int mode = boss.health < BOSS_HEALTH / 3 ? 2 : boss.health < BOSS_HEALTH * 2 / 3 ? 1 : 0;
    if (mode != boss.mode) {
      boss.mode = (uint8_t)mode;
      if (mode == 1) boss.column = (uint8_t)range(0, 2), boss.bungee_in = (int16_t)range(4000, 5000), enter(BS_BUNGEE, CL_BUNGEE_ENTER, -1);
      else boss.row = (uint8_t)range(0, 3), boss.column = (uint8_t)range(0, 2), enter(BS_RV, CL_RV, -1);
    } else if (boss.stomp_in == 0) {
      boss.stomp_in = (int16_t)range(5500, 6500);
      int rows[4], n = 0;
      for (int r = 0; r < 4; r++) {
        bool any = false;
        for (int i = 0; i < B.nplants; i++)
          any |= B.plants[i].row >= r && B.plants[i].row <= r + 1 && B.plants[i].col >= 5 && B.plants[i].health > 0;
        if (any) rows[n++] = r;
      }
      if (n) boss.row = (uint8_t)rows[irand(n)], enter(BS_STOMP, CL_STOMP1 + boss.row, -1);
      else enter(BS_IDLE, CL_IDLE, (float)range(100, 200));
    } else if (boss.bungee_in == 0) {
      if (frand() < 0.25f) boss.row = (uint8_t)range(0, 3), boss.column = (uint8_t)range(0, 2), enter(BS_RV, CL_RV, -1);
      else boss.column = (uint8_t)range(0, 2), boss.bungee_in = (int16_t)range(4000, 5000), enter(BS_BUNGEE, CL_BUNGEE_ENTER, -1);
    } else if (boss.head_in == 0) {
      boss.head_in = (int16_t)range(4000, 5000);
      enter(BS_HEAD_ENTER, CL_HEAD_ENTER, -1);
    } else if (boss.summon_in == 0) {
      boss.row = (uint8_t)range(0, 4);
      boss.summon_in = (int16_t)(boss.mode == 0 ? range(450, 550) : boss.mode == 1 ? range(350, 450) : range(150, 250));
      enter(BS_SPAWN, CL_SPAWN1 + boss.row, -1);
    } else
      enter(BS_IDLE, CL_IDLE, (float)range(100, 200));
  }
}
