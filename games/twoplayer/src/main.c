/* 2 Players: fifteen mini-games for two people on one calculator.
 *
 * Player 1 holds the left of the keyboard (the arrows, and OK for action), player 2 the right
 * (the keys around +, and EXE to fire), both at once; or player 2 is the computer, easy, normal
 * or hard. Pong plays as the 1972 arcade game does; then Air Hockey, Tanks in a maze with
 * shells that bounce off the walls, and Light Cycles. A match is first to a few points, and the
 * matches each side has won are kept for every game.
 *
 * Inside NumPlay it draws with the launcher's strips, shapes and fonts (launcher/src/gfx.c) and
 * saves through its file functions, like Block Blast; on its own (TwoPlayers.nwa) it brings them
 * along. Positions are in 256ths of a pixel and time in milliseconds, but for the round things
 * of Air Hockey. */
#include <eadk.h>
#include <string.h>
#include "../../../launcher/src/gfx.h"
#include "../../common/np_text.h"

#if NP_MODULE /* (np_file_read and np_file_write: NumPlay's, launcher/src/np.h) */
#define np_app_begin()
#define np_app_end() 0
#else
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#define np_file_read ef_read
#define np_file_write ef_write
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "2 Players";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
void np_push(int x, int y, int w, int h, const color_t *px) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, px);
}
#endif

#define WHITE 0xFFFF
#define INK HEX(0x161A33) /* the menus' night blue */
#define GOLD HEX(0xFFC93C)

/* IDs 0..3 are the original games and remain stable in existing saves. */
enum { PONG, HOCKEY, TANKS, CYCLES, QUICK, TUG, FOOTBALL, VOLLEY,
       ARCHERY, DODGE, METEORS, ROCKETS, SUMO, FISHING, MEMORY, NG };
static const char *const NAME[NG] = {T("Pong"), T("Air Hockey"), T("Tanks"), T("Light Cycles"),
  T("Quick Draw"), T("Tug of War"), T("Football"), T("Volley Ball"),
  T("Archery"), T("Dodge Ball"), T("Meteor Run"), T("Rocket Race"),
  T("Sumo"), T("Fishing"), T("Memory Duel")};
static const uint8_t TARGET[NG] = {11, 7, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5};
/* left=1, up=2, down=4, right=8, action=16; used by the keyboard tutorial. */
static const uint8_t CONTROL[NG] = {6,15,31,15,16,16,31,27,22,31,9,18,31,25,15};
static const char *const RULES[NG] = {
  T("Return the ball with your paddle: its middle sends the ball back straight, its ends at a steep angle. The longer a rally lasts, the faster the ball."),
  T("Knock the puck into the other goal. Your mallet stays in your half; hit the puck on the move to send it flying."),
  T("Up/Down drive; Left/Right turn. OK (player 1) or EXE (player 2) fires. Shells bounce off walls and can hit your own tank!"),
  T("Steer your light cycle and make the other one crash into a wall or a trail. It never stops and never turns back."),
  T("Wait for DRAW, then press your action key. A false start loses the round. Simultaneous shots draw."),
  T("Tap your action key to pull the rope. Release between pulls: holding does not repeat. Drag the flag past your line."),
  T("Move around the pitch and kick with your action key. Push the ball into the other goal; rebounds stay in play."),
  T("Move left and right; Up or your action key jumps. Keep the ball off your floor and send it over the net."),
  T("Aim with Up and Down. Hold OK (player 1) or EXE (player 2) to charge; release to shoot. The dotted arc shows your shot. Three hits win the round."),
  T("Move in your half and throw with your action key. Dodge incoming balls. Three hits knock a player out."),
  T("Move left and right to dodge the falling rocks. Both lanes get the same obstacles. Stay alive longer than your rival."),
  T("Tap Up or your action key to boost your rocket. Fly through the gaps; avoid the walls. Both pilots face the same course."),
  T("Move inside the ring. Your action key dashes and pushes your rival. Stay inside: the ring gradually shrinks."),
  T("Move your rod left and right. Hold your action key to lower the hook; release to reel in a fish. Catch five first."),
  T("Watch the arrow sequence, then repeat it with your direction keys. The first correct sequence wins; a wrong arrow loses."),
};

/* twoplayer.sav: the game last chosen, who player 2 is, and the matches won */
static struct {
  uint8_t magic, version, game, cpu; /* cpu: 0 a second player, 1 to 3 the computer, easy to hard */
  uint16_t wins[NG][2][2];           /* [game][against the computer][player 1, player 2] */
} sv;
#define SAVE_NAME "twoplayer.sav"

static color_t strip[SCREEN_W * STRIP_H];
static color_t team[2], light[2], dark[2]; /* player 1 red, player 2 blue */
static uint32_t now, seed, phase_t;        /* now: the game's clock, stopped while paused */
static int ms;                             /* this frame's milliseconds */
static int game, cpu, score[2], phase, winner; /* phase: 0 playing, 1 a point just won, 2 the match over */
static int pad[2], tap[2];                 /* each player's keys held, and just pressed */
enum { B_LEFT = 1, B_UP = 2, B_DOWN = 4, B_RIGHT = 8, B_FIRE = 16 };
/* the keys: left, up, down, right, fire */
static const uint8_t KEYS[2][5] = {
  {eadk_key_left, eadk_key_up, eadk_key_down, eadk_key_right, eadk_key_ok},
  {eadk_key_three, eadk_key_multiplication, eadk_key_ans, eadk_key_minus, eadk_key_exe}};

static uint32_t rnd(void) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  return seed ^= seed << 5;
}
static int rn(int n) { return (int)(rnd() % (uint32_t)n); }
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static float len(float x, float y) { return __builtin_sqrtf(x * x + y * y); }
static uint32_t ago(uint32_t t) { return now - t; }
static bool down(uint64_t keys, int k) { return (uint32_t)(keys >> (k & 32)) >> (k & 31) & 1; }
static bool is_cpu(int p) { return p && cpu; }
static int dirx(int p) { return !!(pad[p] & B_RIGHT) - !!(pad[p] & B_LEFT); }
static int diry(int p) { return !!(pad[p] & B_DOWN) - !!(pad[p] & B_UP); }
/* CPU intentions are sampled, never recomputed with perfect reflexes every tick.
 * Each round starts with a short observation delay, and harder still leaves
 * a human reaction window. The two-human input path never uses this state. */
static struct { uint32_t next; float x,y; int pad; } bot;
static bool bot_due(void) {
  if((int32_t)(now-bot.next)<0)return false;
  bot.next=now+(cpu==1?340:cpu==2?240:160)+rn(cpu==1?140:cpu==2?100:70);
  return true;
}
static void bot_input(void) { pad[1]=bot.pad;tap[1]=0; }
/* sine of a 64th of a turn, times 127 */
static const int8_t SINQ[17] = {0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127};
static int sin64(int a) {
  int q = a & 15, v = a & 16 ? SINQ[16 - q] : SINQ[q];
  return a & 32 ? -v : v;
}
static int cos64(int a) { return sin64(a + 16); }

static char *num(char *o, uint32_t v) { /* v in o, then the end of it */
  char t[10];
  int n = 0;
  do t[n++] = (char)('0' + v % 10); while (v /= 10);
  while (n) *o++ = t[--n];
  *o = 0;
  return o;
}
/* text with a shadow, to read over anything */
static void say(const np_font_t *f, int cx, int base, const char *s, color_t c) {
  gfx_text_center(f, cx + 1, base + 2, s, 0, 150);
  gfx_text_center(f, cx, base, s, c, 256);
}
static const char *who(int p) {
  return p ? (cpu ? T("Computer") : T("Player 2")) : (cpu ? T("You") : T("Player 1"));
}
static void box(int x, int y, int w, int h, int r, color_t c) { gfx_rrect(x, y, w, h, r, c, 256); }
static void disc(int x, int y, int r, color_t c) { gfx_circle(x, y, r, c, 256); }

/* A match: points, then the next point or round once the last one has been seen. */
static void point(int w) {
  if (phase) return;
  winner = w, phase = 1, phase_t = now;
  if (w < 2) score[w]++;
}

/* ------------------------------------------------------------ Pong
   The 1972 arcade game: a paddle has eight segments, the two in the middle send the ball back
   straight, the others at steeper and steeper angles; the ball goes 25% faster after 4 hits,
   50% after 12, and a little faster with every hit after that, until a point is won. */
#define PH 30  /* a paddle */
#define PW 6
#define BS 6   /* the ball, square */
#define PTOP 8 /* the walls */
#define PBOT 232
static const int PX[2] = {24, SCREEN_W - 24 - PW};
static const int8_t SEG[8] = {-10, -7, -4, 0, 0, 4, 7, 10}; /* tenths of the speed, up or down */
static int py[2], bx, by, bvx, bvy, aim, hits;
static uint32_t serve_at; /* the ball waits in the middle until then (0: in play) */

/* the ball's speed across, in 256ths of a pixel a millisecond (38: 150 pixels a second) */
static int pong_speed(void) { return hits >= 12 ? clampi(46 + hits, 58, 84) : hits >= 4 ? 48 : 38; }
static void pong_start(void) { /* the ball is served towards the one who lost the point */
  int to = winner < 2 ? 1 - winner : rn(2);
  hits = aim = 0;bot.y=120<<8;
  bx = (160 - BS / 2) << 8, by = (70 + rn(100)) << 8, bvx = to ? 1 : -1, bvy = rn(9) - 4;
  serve_at = now + 900;
  if (!score[0] && !score[1]) py[0] = py[1] = (120 - PH / 2) << 8;
}
/* the computer's paddle (player 2): to where the ball will come, off centre by its aim */
static int pong_cpu(void) {
  static const uint8_t SPEED[4]={0,20,30,40};
  if(bot_due()){
    int to=120<<8;
    if(bvx>0&&!serve_at&&bx>(cpu==1?190<<8:cpu==2?145<<8:90<<8)){
      int span=(PBOT-BS-PTOP)<<8,y=by-(PTOP<<8)+bvy*(((PX[1]-BS)<<8)-bx)/bvx;
      for(int i=0;i<8&&(y<0||y>span);i++)y=y<0?-y:2*span-y;
      to=y+((PTOP+BS/2)<<8)-aim;
    }
    int error=cpu==1?20:cpu==2?12:7;
    bot.y=clampi(to+(rn(error*2+1)-error)*256,PTOP<<8,PBOT<<8);
  }
  int delta=(int)bot.y-py[1]-(PH<<7);
  return clampi(delta/40,-SPEED[cpu],SPEED[cpu]);
}
static void pong_step(void) {
  for (int p = 0; p < 2; p++)
    py[p] = clampi(py[p] + (is_cpu(p) ? pong_cpu() : diry(p) * 64) * ms, PTOP << 8, (PBOT - PH) << 8);
  if (phase || (int32_t)(now - serve_at) < 0) return;
  if (serve_at) serve_at = 0, bvx *= pong_speed(), bvy = bvy * pong_speed() / 10;
  for (int i = 0; i < ms; i++) {
    bx += bvx, by += bvy;
    if (by < PTOP << 8 || by > (PBOT - BS) << 8) by -= 2 * bvy, bvy = -bvy;
    int p = bvx > 0, x = bx >> 8, y = by >> 8, top = py[p] >> 8;
    if (x + BS > PX[p] && x < PX[p] + PW && y + BS > top && y < top + PH) {
      int s = (++hits, pong_speed());
      bvx = p ? -s : s, bvy = SEG[clampi((y + BS - top) * 8 / (PH + BS), 0, 7)] * s / 10;
      bx = (p ? PX[p] - BS : PX[p] + PW) << 8;
      /* where the computer means to take the next one: off centre, by how much it misjudges */
      if (!p) aim = cpu == 3 ? (rn(2) ? 2816 : -2816) + rn(1536) - 768 : (rn(512) - 256) * (cpu == 1 ? 25 : 14);
    }
    if (x < -BS - 10 || x > SCREEN_W + 10) {
      point(x < 0);
      return;
    }
  }
}
/* the arcade game's digits: three by five blocks */
#define D(a, b, c, d, e) ((a) << 12 | (b) << 9 | (c) << 6 | (d) << 3 | (e))
static const uint16_t DIGIT[10] = {D(7, 5, 5, 5, 7), D(1, 1, 1, 1, 1), D(7, 1, 7, 4, 7), D(7, 1, 7, 1, 7),
                                   D(5, 5, 7, 1, 1), D(7, 4, 7, 1, 7), D(7, 4, 7, 5, 7), D(7, 1, 1, 1, 1),
                                   D(7, 5, 7, 5, 7), D(7, 5, 7, 1, 7)};
static void pong_paint(void) {
  gfx_fill(0, 0, SCREEN_W, SCREEN_H, 0);
  gfx_fill(0, PTOP - 4, SCREEN_W, 4, WHITE);
  gfx_fill(0, PBOT, SCREEN_W, 4, WHITE);
  for (int y = PTOP + 4; y < PBOT; y += 16) gfx_fill(158, y, 4, 8, WHITE);
  for (int p = 0; p < 2; p++) {
    gfx_fill(PX[p], py[p] >> 8, PW, PH, WHITE);
    for (int v = score[p], n = v > 9, d = n; d >= 0; d--, v /= 10)
      for (int b = 0; b < 15; b++)
        if (DIGIT[v % 10] >> (14 - b) & 1) gfx_fill(p * 100 + 103 - n * 10 + d * 20 + b % 3 * 5, 18 + b / 3 * 5, 5, 5, WHITE);
  }
  if (!phase && (!serve_at || ago(serve_at) % 300 < 200)) gfx_fill(bx >> 8, by >> 8, BS, BS, WHITE);
}

/* ------------------------------------------------------------ Air Hockey */
#define RX0 10 /* the table */
#define RX1 310
#define RY0 30
#define RY1 232
#define RYM 131
#define GY0 103 /* the goals */
#define GY1 159
#define MR 13 /* a mallet */
#define QR 9  /* the puck */
static float mx[2], my[2], mvx[2], mvy[2], qx, qy, qvx, qvy, qaim;

static void hockey_start(void) { /* the puck to the one who was scored on */
  int to = winner < 2 ? 1 - winner : rn(2);
  for (int p = 0; p < 2; p++) mx[p] = p ? RX1 - 40 : RX0 + 40, my[p] = RYM, mvx[p] = mvy[p] = 0;
  bot.x=RX1-40;bot.y=RYM;
  qx = to ? 220 : 100, qy = RYM, qvx = qvy = 0, qaim = (float)(rn(44) - 22); /* where the computer aims */
}
/* the puck off something round at (x, y) moving at (vx, vy) */
static void knock(float x, float y, float vx, float vy, float r) {
  float dx = qx - x, dy = qy - y, d = len(dx, dy), R = r + QR;
  if (d >= R || d < 0.01f) return;
  dx /= d, dy /= d;
  qx = x + dx * R, qy = y + dy * R;
  float vn = (qvx - vx) * dx + (qvy - vy) * dy;
  if (vn < 0) qvx -= 1.85f * vn * dx, qvy -= 1.85f * vn * dy;
}
static void hockey_step(void) {
  static const float V[4] = {0, 90, 130, 175};
  for (int p = 0; p < 2; p++) {
    float tx, ty, k = clampf(ms * 0.014f, 0, 1);
    if (is_cpu(p)) { /* behind the puck to shoot it at the other goal, or guarding its own */
      if(bot_due()) {
      tx = RX1 - 36, ty = RYM + (qy - RYM) / 2;
      if (qx > 160 - QR && !phase) {
        float ax = RX0 - qx, ay = RYM + qaim - qy, l = len(ax, ay);
        ax /= l, ay /= l;
        tx = qx - ax * (MR + QR + 3), ty = qy - ay * (MR + QR + 3);
        if (mx[1] < qx + 2) tx = qx + MR + QR + 10, ty = qy + (qy > RYM ? -28 : 28); /* in front of it: go round */
        else if (len(tx - mx[1], ty - my[1]) < 7) tx = qx + ax * 24, ty = qy + ay * 24; /* strike */
      }
      bot.x=tx;bot.y=ty+(rn(cpu==1?37:cpu==2?23:13)-(cpu==1?18:cpu==2?11:6));
      }
      tx=bot.x-mx[1];ty=bot.y-my[1];
      float l = len(tx, ty) + 0.01f, v = V[cpu] * (l < 16 ? l / 16 : 1) / l;
      tx *= v, ty *= v;
    } else {
      int x = dirx(p), y = diry(p);
      float s = x && y ? 175 : 245;
      tx = x * s, ty = y * s;
    }
    mvx[p] += (tx - mvx[p]) * k, mvy[p] += (ty - mvy[p]) * k;
    mx[p] = clampf(mx[p] + mvx[p] * ms / 1000, p ? 160 + MR : RX0 + MR, p ? RX1 - MR : 160 - MR);
    my[p] = clampf(my[p] + mvy[p] * ms / 1000, RY0 + MR, RY1 - MR);
  }
  if (phase) return;
  for (int i = 0; i < ms; i++) {
    qx += qvx / 1000, qy += qvy / 1000;
    /* the boards: back on their edge, and off them only when moving into them (a mallet can push the
       puck into a board, and a slow puck flipped back and forth there stayed in it) */
    if (qy < RY0 + QR) qy = RY0 + QR, qvy = qvy < 0 ? -qvy * 0.9f : qvy;
    else if (qy > RY1 - QR) qy = RY1 - QR, qvy = qvy > 0 ? -qvy * 0.9f : qvy;
    if (qy < GY0 || qy > GY1) {
      if (qx < RX0 + QR) qx = RX0 + QR, qvx = qvx < 0 ? -qvx * 0.9f : qvx;
      else if (qx > RX1 - QR) qx = RX1 - QR, qvx = qvx > 0 ? -qvx * 0.9f : qvx;
    }
    if (qx < RX0 - QR || qx > RX1 + QR) {
      point(qx < 160);
      return;
    }
    for (int k = 0; k < 4; k++) knock(k & 1 ? RX1 : RX0, k & 2 ? GY1 : GY0, 0, 0, 0); /* the goal posts */
    for (int p = 0; p < 2; p++) knock(mx[p], my[p], mvx[p], mvy[p], MR);
  }
  float s = len(qvx, qvy), f = 1 - 0.00035f * ms;
  if (s * f > 560) f = 560 / s;
  qvx *= f, qvy *= f;
}
static void hockey_paint(void) {
  const color_t RED = HEX(0xE25555), BLUE = HEX(0x5C8FE6), RIM = HEX(0x8D9BB0), ICE = HEX(0xEDF4FA);
  gfx_fill(0, 22, SCREEN_W, SCREEN_H - 22, INK);
  box(RX0 - 5, RY0 - 5, RX1 - RX0 + 10, RY1 - RY0 + 10, 14, RIM);
  box(RX0, RY0, RX1 - RX0, RY1 - RY0, 10, ICE);
  gfx_fill(159, RY0, 3, RY1 - RY0, RED);
  for (int k = 0; k < 3; k++) { /* the creases round the goals, and the circle in the middle */
    int c = k == 1, d = 80 - 20 * c;
    gfx_rrect_outline(k * 150 - 30 + 10 * c, RYM - d / 2, d, d, d / 2, 2, c ? RED : BLUE, 220);
  }
  for (int k = 0; k < 2; k++) { /* the rim over the creases, and the goals in it */
    gfx_fill(k ? RX1 + 5 : 0, RY0, k ? SCREEN_W - RX1 - 5 : RX0 - 5, RY1 - RY0, INK);
    gfx_fill(k ? RX1 : RX0 - 5, RY0, 5, RY1 - RY0, RIM);
    gfx_fill(k ? RX1 : RX0 - 6, GY0, 6, GY1 - GY0, HEX(0x0B0E1C));
  }
  if (!phase) disc((int)qx + 2, (int)qy + 3, QR, gfx_mix(ICE, 0, 10)), disc((int)qx, (int)qy, QR, HEX(0x22252E)),
              disc((int)qx, (int)qy, QR - 3, HEX(0x3B3F4C));
  for (int p = 0; p < 2; p++) {
    int x = (int)mx[p], y = (int)my[p];
    disc(x + 2, y + 3, MR, gfx_mix(ICE, 0, 10));
    disc(x, y, MR, dark[p]);
    disc(x, y, MR - 3, team[p]);
    disc(x, y, 6, dark[p]);
    disc(x - 1, y - 1, 4, light[p]);
  }
}

/* ------------------------------------------------------------ Tanks
   A maze of 8 x 6 cells: a random one with every cell reached, then a third of its walls left
   knocked down so there are ways round. Walls are 4 pixels thick, on the cells' edges. A tank
   faces one of 64 ways (ta: 256ths of them); shells fly at 124 pixels a second. */
#define TCS 35 /* a cell */
#define TX0 20
#define TY0 26
#define TCOLS 8
#define TROWS 6
#define TR 10 /* a tank, for walls and shells */
#define SHELLS 10
static uint8_t hwall[TROWS + 1][TCOLS], vwall[TROWS][TCOLS + 1];
static int tx_[2], ty_[2], ta[2], ai_aim;
static bool dead[2];
static uint32_t dead_t[2], ai_t, ai_shot;
static struct { int x, y, vx, vy, life, own; } sh[SHELLS]; /* own -1: none */

static bool H(int r, int c) { return r >= 0 && r <= TROWS && c >= 0 && c < TCOLS && hwall[r][c]; }
static bool V(int r, int c) { return r >= 0 && r < TROWS && c >= 0 && c <= TCOLS && vwall[r][c]; }
static int tcell(int x, int y) { /* the cell at (x, y), in 256ths of a pixel */
  return clampi(((y >> 8) - TY0) / TCS, 0, TROWS - 1) * TCOLS + clampi(((x >> 8) - TX0) / TCS, 0, TCOLS - 1);
}
/* inside a wall (or out of the maze)? */
static bool solid(int x, int y) {
  int k = tcell(x, y), r = k / TCOLS, c = k % TCOLS, fx = (x >> 8) - TX0 - c * TCS, fy = (y >> 8) - TY0 - r * TCS;
  bool L = fx < 2, R = fx >= TCS - 2, T = fy < 2, B = fy >= TCS - 2;
  if ((T && H(r, c)) || (B && H(r + 1, c)) || (L && V(r, c)) || (R && V(r, c + 1))) return true;
  /* the corners, where any wall meeting there fills them */
  int gr = r + B, gc = c + R;
  return (L || R) && (T || B) && (H(gr, gc - 1) || H(gr, gc) || V(gr - 1, gc) || V(gr, gc));
}
static bool open_way(int k, int d) { /* from cell k to the next one in direction d (right, down, left, up) */
  int r = k / TCOLS, c = k % TCOLS;
  return d == 0 ? !V(r, c + 1) : d == 1 ? !H(r + 1, c) : d == 2 ? !V(r, c) : !H(r, c);
}
static const int8_t STEP[4] = {1, TCOLS, -1, -TCOLS};
static bool near(int x, int y, int p, int r) {
  x = (x - tx_[p]) >> 8, y = (y - ty_[p]) >> 8;
  return x * x + y * y < r * r;
}
static void tanks_start(void) {
  uint8_t seen[TROWS * TCOLS] = {0};
  for (uint8_t *w = &hwall[0][0]; w < &hwall[0][0] + sizeof hwall; w++) *w = 1;
  for (uint8_t *w = &vwall[0][0]; w < &vwall[0][0] + sizeof vwall; w++) *w = 1;
  /* a random walk, knocking down the wall into every cell it reaches first: any maze as likely as
     any other */
  int k = rn(TROWS * TCOLS), left = TROWS * TCOLS - 1;
  for (seen[k] = 1; left;) {
    int d = rn(4), j = k + STEP[d], r = k / TCOLS, c = k % TCOLS;
    if ((d == 0 && c == TCOLS - 1) || (d == 2 && !c) || j < 0 || j >= TROWS * TCOLS) continue;
    if (!seen[j]) *(d & 1 ? &hwall[r + (d == 1)][c] : &vwall[r][c + (d == 0)]) = 0, seen[j] = 1, left--;
    k = j;
  }
  for (int r = 1; r < TROWS; r++)
    for (int c = 1; c < TCOLS; c++) {
      if (!rn(3)) hwall[r][c] = 0;
      if (!rn(3)) vwall[r][c] = 0;
    }
  for (int p = 0; p < 2; p++) {
    int c = p ? TCOLS - 1 - rn(3) : rn(3);
    tx_[p] = (TX0 + c * TCS + TCS / 2) << 8, ty_[p] = (TY0 + rn(TROWS) * TCS + TCS / 2) << 8;
    ta[p] = p ? 32 << 8 : 0, dead[p] = false;
  }
  for (int i = 0; i < SHELLS; i++) sh[i].own = -1;
  ai_t = ai_shot = now;
}
static bool tank_stuck(int p, int x, int y) {
  if (!dead[1 - p] && near(x, y, 1 - p, 2 * TR)) return true;
  for (int i = 0; i < 64; i += 4)
    if (solid(x + cos64(i) * TR * 2, y + sin64(i) * TR * 2)) return true;
  return false;
}
static void fire(int p) {
  int n = 0, k = -1, a = ta[p] >> 8;
  for (int i = 0; i < SHELLS; i++)
    if (sh[i].own == p) n++;
    else if (sh[i].own < 0) k = i;
  if (n >= 5 || k < 0) return;
  sh[k].x = tx_[p] + cos64(a) * (TR + 3) * 2, sh[k].y = ty_[p] + sin64(a) * (TR + 3) * 2;
  sh[k].vx = cos64(a), sh[k].vy = sin64(a), sh[k].life = 9000, sh[k].own = p;
}
/* a shell's way for h ms, off the walls it meets */
static void roll(int *x, int *y, int *vx, int *vy, int h) {
  int nx = *x + *vx * h / 4, ny = *y + *vy * h / 4;
  if (!solid(nx, ny)) {
    *x = nx, *y = ny;
    return;
  }
  bool a = solid(nx, *y), b = solid(*x, ny);
  if (a == b || a) *vx = -*vx;
  if (a == b || b) *vy = -*vy;
}
/* Would a shell fired at a from the computer's tank find the player's, bouncing at most b
   times, before its own? */
static bool shot_finds(int a, int b) {
  int x = tx_[1], y = ty_[1], vx = cos64(a), vy = sin64(a);
  for (int i = 0; i < 400; i++) {
    int ox = vx, oy = vy;
    roll(&x, &y, &vx, &vy, 16);
    if ((ox != vx || oy != vy) && --b < 0) return false;
    if (near(x, y, 0, TR)) return true;
    if (i > 8 && near(x, y, 1, TR + 3)) return false;
  }
  return false;
}
/* the computer's tank: a shot when there is one to take, else the way to the player, cell by
   cell through the maze (found breadth first from the player) */
static void tanks_cpu(int *v, int *turn) {
  static const int16_t RELOAD[4] = {0, 2300, 1700, 1200};
  static const int8_t BOUNCES[4] = {0, 0, 0, 1};
  int dir = ta[1] >> 8, want = dir;
  if (ago(ai_t) > (uint32_t)(cpu==1?700:cpu==2?500:350)) {
    ai_t = now, ai_aim = -1;
    for (int i = 0; i < 64 && ai_aim < 0; i++) { /* the nearest to where it faces first */
      int a = (dir + (i & 1 ? 1 : -1) * ((i + 1) / 2)) & 63;
      if (shot_finds(a, BOUNCES[cpu])) ai_aim = (a + rn(cpu==1?11:cpu==2?7:3) - (cpu==1?5:cpu==2?3:1)) & 63;
    }
  }
  if (ai_aim >= 0) {
    want = ai_aim;
    if (want == dir && ago(ai_shot) > (uint32_t)RELOAD[cpu]) fire(1), ai_shot = now, ai_t = now - 200;
  } else {
    uint8_t dist[TROWS * TCOLS], q[TROWS * TCOLS];
    int me = tcell(tx_[1], ty_[1]), you = tcell(tx_[0], ty_[0]), n = 1, next = you;
    for (int i = 0; i < TROWS * TCOLS; i++) dist[i] = 255;
    dist[you] = 0, q[0] = (uint8_t)you;
    for (int h = 0; h < n; h++)
      for (int d = 0; d < 4; d++)
        if (open_way(q[h], d) && dist[q[h] + STEP[d]] == 255) dist[q[h] + STEP[d]] = dist[q[h]] + 1, q[n++] = (uint8_t)(q[h] + STEP[d]);
    for (int d = 0; d < 4; d++)
      if (me != you && open_way(me, d) && dist[me + STEP[d]] < dist[me]) next = me + STEP[d];
    int gx = ((TX0 + next % TCOLS * TCS + TCS / 2) << 8) - tx_[1], gy = ((TY0 + next / TCOLS * TCS + TCS / 2) << 8) - ty_[1];
    if (next == you) gx = tx_[0] - tx_[1], gy = ty_[0] - ty_[1];
    for (int a = 0, best = -(1 << 30); a < 64; a++) { /* the way of the 64 nearest to it */
      int d = (gx >> 8) * cos64(a) + (gy >> 8) * sin64(a);
      if (d > best) best = d, want = a;
    }
    *v = clampi(6 - (((want - dir + 32) & 63) - 32) * (((want - dir + 32) & 63) - 32), 0, 1) * (cpu==1?45:cpu==2?55:64);
  }
  int d = ((want - dir + 32) & 63) - 32;
  *turn = d > 0 ? 4+cpu : d < 0 ? -4-cpu : 0;
}
static void tanks_step(void) {
  for (int p = 0; p < 2; p++) {
    if (dead[p]) continue;
    int v = 0, turn = 0;
    if (is_cpu(p)) tanks_cpu(&v, &turn);
    else {
      v = diry(p) < 0 ? 70 : diry(p) * -50, turn = dirx(p) * 8;
      if (tap[p] & B_FIRE) fire(p);
    }
    ta[p] = (ta[p] + turn * ms) & 0x3FFF; /* 8 a ms: half a turn a second */
    int x = tx_[p], y = ty_[p], dx = cos64(ta[p] >> 8) * v * ms / 496, dy = sin64(ta[p] >> 8) * v * ms / 496;
    if (!tank_stuck(p, x + dx, y + dy)) tx_[p] += dx, ty_[p] += dy;
    else if (!tank_stuck(p, x + dx, y)) tx_[p] += dx;
    else if (!tank_stuck(p, x, y + dy)) ty_[p] += dy;
  }
  for (int i = 0; i < SHELLS; i++)
    for (int j = 0; j < ms && sh[i].own >= 0; j += 8) {
      roll(&sh[i].x, &sh[i].y, &sh[i].vx, &sh[i].vy, ms - j < 8 ? ms - j : 8);
      if ((sh[i].life -= 8) <= 0) sh[i].own = -1;
      for (int p = 0; p < 2; p++)
        if (!dead[p] && near(sh[i].x, sh[i].y, p, TR - 1)) dead[p] = true, dead_t[p] = now, sh[i].own = -1;
    }
  /* a round ends a moment after a tank is hit: the other may be hit too, a draw */
  int k = dead[0] && (!dead[1] || dead_t[0] < dead_t[1]) ? 0 : 1; /* the first one hit */
  if (dead[k] && ago(dead_t[k]) > 1200) point(dead[0] && dead[1] ? 2 : !dead[0] ? 0 : 1);
}
/* a rectangle from l0 to l1 ahead of (x, y) the way a faces, w to either side (quarter pixels) */
static void quad(int x, int y, int a, int l0, int l1, int w, color_t col) {
  int c = cos64(a), s = sin64(a), vx[4], vy[4];
  for (int i = 0; i < 4; i++) {
    int l = i == 1 || i == 2 ? l1 : l0, e = i < 2 ? -w : w;
    vx[i] = 4 * x + (c * l + s * e) / 127, vy[i] = 4 * y + (s * l - c * e) / 127;
  }
  for (int row = gfx_y0; row < gfx_y1; row++) {
    int yc = 4 * row + 2, lo = 1 << 20, hi = -lo;
    for (int i = 0; i < 4; i++) {
      int j = (i + 1) & 3;
      if ((vy[i] <= yc) != (vy[j] <= yc)) {
        int xx = vx[i] + (yc - vy[i]) * (vx[j] - vx[i]) / (vy[j] - vy[i]);
        lo = xx < lo ? xx : lo, hi = xx > hi ? xx : hi;
      }
    }
    if (hi > lo) gfx_fill((lo + 2) >> 2, row, ((hi + 2) >> 2) - ((lo + 2) >> 2), 1, col);
  }
}
static void tank(int x, int y, int a, int p) {
  quad(x + 1, y + 2, a, -32, 32, 28, HEX(0xB9BBB6)); /* its shadow */
  quad(x, y, a, -32, 32, 28, dark[p]);
  quad(x, y, a, -24, 24, 18, team[p]);
  quad(x, y, a, 0, 52, 6, dark[p]);
  disc(x, y, 4, dark[p]);
  disc(x, y, 2, light[p]);
}
static void tanks_paint(void) {
  gfx_fill(0, 22, SCREEN_W, SCREEN_H - 22, HEX(0xE6E6E1));
  for (int r = 0; r <= TROWS; r++)
    for (int c = 0; c <= TCOLS; c++) {
      if (H(r, c)) gfx_fill(TX0 + c * TCS - 2, TY0 + r * TCS - 2, TCS + 4, 4, HEX(0x3A3F4B));
      if (V(r, c)) gfx_fill(TX0 + c * TCS - 2, TY0 + r * TCS - 2, 4, TCS + 4, HEX(0x3A3F4B));
    }
  for (int i = 0; i < SHELLS; i++)
    if (sh[i].own >= 0) disc(sh[i].x >> 8, sh[i].y >> 8, 2, HEX(0x1E1F24));
  for (int p = 0; p < 2; p++) {
    int x = tx_[p] >> 8, y = ty_[p] >> 8, t = (int)ago(dead_t[p]);
    if (!dead[p]) {
      tank(x, y, ta[p] >> 8, p);
      continue;
    }
    gfx_circle(x, y, 9, HEX(0x55524C), 150); /* what is left of it */
    if (t < 700) gfx_circle(x, y, 6 + t / 30, HEX(0xFF8A2A), 256 - t * 256 / 700), gfx_circle(x, y, 3 + t / 50, HEX(0xFFE066), 256 - t * 256 / 700);
  }
}

/* ------------------------------------------------------------ Light Cycles */
#define CW 80 /* the grid, 4 pixels a cell */
#define CH 54
#define CY0 24
#define QMAX 1024
static uint8_t cell[CH][CW], mark[CH][CW], gen;
static uint16_t queue[QMAX];
static int hx[2], hy[2], hd[2], turns[2][2], nturns[2];
static bool crashed[2];
static uint32_t tick_t, round_t;
static const int8_t DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1}; /* right, down, left, up */

static void cycles_start(void) { /* side by side, each on its own lane, the other's way */
  for (uint8_t *c = &cell[0][0]; c < &cell[0][0] + CW * CH; c++) *c = 0;
  for (int p = 0; p < 2; p++) {
    hx[p] = p ? CW - 17 : 16, hy[p] = p ? CH / 2 + 8 : CH / 2 - 8, hd[p] = p ? 2 : 0, nturns[p] = 0, crashed[p] = false;
    cell[hy[p]][hx[p]] = (uint8_t)(p + 1);
  }
  tick_t = round_t = now;
}
static bool free_at(int x, int y) { return x >= 0 && y >= 0 && x < CW && y < CH && !cell[y][x]; }
/* the free cells the computer could still reach from (x, y), up to cap */
static int room(int x, int y, int cap) {
  if (!free_at(x, y)) return 0;
  if (!++gen)
    for (uint8_t *m = &mark[0][0]; m < &mark[0][0] + CW * CH; m++) *m = 0;
  int n = 1;
  queue[0] = (uint16_t)(y * CW + x), mark[y][x] = gen;
  for (int h = 0; h < n && n < cap; h++)
    for (int d = 0; d < 4; d++) {
      int nx = queue[h] % CW + DX[d], ny = queue[h] / CW + DY[d];
      if (free_at(nx, ny) && mark[ny][nx] != gen && n < QMAX) mark[ny][nx] = gen, queue[n++] = (uint16_t)(ny * CW + nx);
    }
  return n;
}
/* Choose space at human reaction intervals, without predicting the player's
   future cells. Look a little ahead so slower decisions can still avoid a wall. */
static int cycles_cpu(void) {
  if(!bot_due())return hd[1];
  static const int16_t CAP[4] = {0, 12, 70, 220};
  int best = hd[1], bv = -(1 << 30);
  for (int t = 0; t < 3; t++) {
    int d = (hd[1] + t + (t == 2)) & 3, x = hx[1] + DX[d], y = hy[1] + DY[d], r = room(x, y, CAP[cpu]);
    int v = r * 8 + (t ? 0 : 6) + rn(cpu == 1 ? 120 : 6);
    int ahead=0;
    while(ahead<7&&free_at(x+ahead*DX[d],y+ahead*DY[d]))ahead++;
    v+=ahead*48;
    if (!r) v = -(1 << 29);
    if (v > bv) bv = v, best = d;
  }
  return best;
}
static void cycles_step(void) {
  static const uint8_t BIT[4] = {B_RIGHT, B_DOWN, B_LEFT, B_UP};
  for (int p = 0; p < 2; p++)
    for (int d = 0; d < 4; d++) { /* a turn waits its turn: two can wait */
      int last = nturns[p] ? turns[p][nturns[p] - 1] : hd[p];
      if (tap[p] & BIT[d] && d != last && (d ^ last) != 2 && nturns[p] < 2) turns[p][nturns[p]++] = d;
    }
  /* a step every 70 ms, down to 42 as the round goes on */
  uint32_t period = 70 - (ago(round_t) < 28000 ? ago(round_t) / 1000 : 28);
  if (phase || ago(tick_t) < period) return;
  tick_t = ago(tick_t) > 100 ? now : tick_t + period;
  int nx[2], ny[2];
  for (int p = 0; p < 2; p++) {
    if (is_cpu(p)) hd[p] = cycles_cpu();
    else if (nturns[p]) hd[p] = turns[p][0], turns[p][0] = turns[p][1], nturns[p]--;
    nx[p] = hx[p] + DX[hd[p]], ny[p] = hy[p] + DY[hd[p]], crashed[p] = !free_at(nx[p], ny[p]);
  }
  if (nx[0] == nx[1] && ny[0] == ny[1]) crashed[0] = crashed[1] = true;
  for (int p = 0; p < 2; p++)
    if (!crashed[p]) hx[p] = nx[p], hy[p] = ny[p], cell[hy[p]][hx[p]] = (uint8_t)(p + 1);
  if (crashed[0] || crashed[1]) point(crashed[0] && crashed[1] ? 2 : crashed[0]);
}
static void cycles_paint(void) {
  gfx_fill(0, 22, SCREEN_W, SCREEN_H - 22, HEX(0x060A18));
  for (int i = 0; i < SCREEN_W; i += 16) gfx_fill(i, CY0, 1, SCREEN_H, HEX(0x111B36)), gfx_fill(0, CY0 + i, SCREEN_W, 1, HEX(0x111B36));
  for (int r = clampi((gfx_y0 - CY0) / 4, 0, CH); r < clampi((gfx_y1 - CY0) / 4 + 1, 0, CH); r++)
    for (int c = 0; c < CW; c++) {
      int v = cell[r][c];
      if (!v) continue;
      int l = c && cell[r][c - 1] == v, u = r && cell[r - 1][c] == v; /* its light, joined to the trail before */
      gfx_fill(c * 4, CY0 + r * 4, 4, 4, team[v - 1]);
      gfx_fill(c * 4 + 1 - 2 * l, CY0 + r * 4 + 1 - 2 * u, 2 + 2 * l, 2 + 2 * u, light[v - 1]);
    }
  for (int p = 0; p < 2; p++) {
    int x = hx[p] * 4 + 2, y = CY0 + hy[p] * 4 + 2, t = (int)ago(phase_t);
    gfx_circle(x, y, 6, light[p], 90);
    gfx_fill(x - 2, y - 2, 4, 4, WHITE);
    if (crashed[p] && phase) { /* a flash where it hit */
      x += DX[hd[p]] * 3, y += DY[hd[p]] * 3;
      if (t < 800) gfx_circle(x, y, 4 + t / 25, WHITE, 230 - t * 230 / 800);
      disc(x, y, 3, light[p]);
    }
  }
}

#include "party.h"
static void (*const START[NG])(void) = {pong_start,hockey_start,tanks_start,cycles_start,
  quick_start,tug_start,football_start,volley_start,archery_start,dodge_start,
  meteors_start,rockets_start,sumo_start,fishing_start,memory_start};
static void (*const STEP_[NG])(void) = {pong_step,hockey_step,tanks_step,cycles_step,
  quick_step,tug_step,football_step,volley_step,archery_step,dodge_step,
  meteors_step,rockets_step,sumo_step,fishing_step,memory_step};
static void (*const PAINT[NG])(void) = {pong_paint,hockey_paint,tanks_paint,cycles_paint,
  quick_paint,tug_paint,football_paint,volley_paint,archery_paint,dodge_paint,
  meteors_paint,rockets_paint,sumo_paint,fishing_paint,memory_paint};

/* ------------------------------------------------------------ screens */
enum { S_MENU, S_RULES, S_PLAY, S_PAUSE, S_ASK, S_OVER, S_SAVE_ERROR };
static int screen, sel, back_to;

#define SAVE_BYTES (8+NG*8)
static uint32_t save_crc(const uint8_t*d,unsigned n) {
  uint32_t c=~0u;while(n--){c^=*d++;for(int k=0;k<8;k++)c=c>>1^((c&1)?0xEDB88320u:0);}return ~c;
}
static void save_encode(uint8_t*out) {
  out[0]='P';out[1]=3;out[2]=(uint8_t)game;out[3]=(uint8_t)cpu;
  for(unsigned g=0;g<NG;g++)for(unsigned m=0;m<2;m++)for(unsigned p=0;p<2;p++){
    unsigned i=g*4+m*2+p;uint16_t count=sv.wins[g][m][p];
    out[4+i*2]=(uint8_t)count;out[5+i*2]=(uint8_t)(count>>8);
  }
  uint32_t crc=save_crc(out,SAVE_BYTES-4);for(int i=0;i<4;i++)out[SAVE_BYTES-4+i]=(uint8_t)(crc>>(8*i));
}
static bool save_decode(const uint8_t*d,unsigned n) {
  if(!d||n<4||d[0]!='P'||d[3]>3)return false;
  unsigned count;
  if(d[1]==1&&n==36&&d[2]<4)count=4;
  else if((d[1]==2&&n==136&&d[2]<16)||(d[1]==3&&n==SAVE_BYTES&&d[2]<NG)){
    uint32_t crc=0;for(int i=0;i<4;i++)crc|=(uint32_t)d[n-4+i]<<(8*i);
    if(crc!=save_crc(d,n-4))return false;
    count=d[1]==2?16:NG;
  }else return false;
  memset(&sv,0,sizeof sv);sv.magic='P';sv.version=3;sv.game=d[1]==2?(d[2]==14?PONG:d[2]==15?MEMORY:d[2]):d[2];sv.cpu=d[3];
  for(unsigned g=0;g<count;g++)for(unsigned m=0;m<2;m++)for(unsigned p=0;p<2;p++){
    if(d[1]==2&&g==14)continue;
    unsigned dest=d[1]==2&&g==15?MEMORY:g;
    unsigned i=g*4+m*2+p;sv.wins[dest][m][p]=(uint16_t)(d[4+i*2]|d[5+i*2]<<8);
  }
  return true;
}
static bool save(void) {
  uint8_t packed[SAVE_BYTES];save_encode(packed);
  return np_file_write(SAVE_NAME,packed,sizeof packed);
}
static void start_round(void) {
  memset(&bot,0,sizeof bot);bot.next=now+(cpu==1?750:cpu==2?600:450);START[game]();
}
static int page_game(int direction) {
  int page=(game/4+direction+(NG+3)/4)%((NG+3)/4);
  return clampi(page*4+game%4,0,NG-1);
}
static void new_match(void) {
  score[0] = score[1] = 0, phase = 0, winner = 2, screen = S_PLAY;
  start_round();
}
static void match_step(void) {
  if(cpu)pad[1]=tap[1]=0; /* EXE is a solo action alias, never an input to the bot. */
  STEP_[game]();
  if(phase==1&&ago(phase_t)>1500){
    phase=0;
    if(score[0]<TARGET[game]&&score[1]<TARGET[game])start_round();
    else{
      phase=2;screen=S_OVER;sel=0;
      uint16_t*w=&sv.wins[game][cpu>0][score[1]>score[0]];if(*w<65535)(*w)++;
      save();
    }
  }
}
static const char *opponent(void) {
  static const char *const O[4] = {T("Player 2"), T("Computer: easy"), T("Computer: normal"), T("Computer: hard")};
  return O[cpu];
}

static void tri(int x, int y, int d, color_t c) { /* a little arrow, pointing right, down, left or up */
  for (int i = 0; i < 4; i++) {
    int a = d < 2 ? 3 - i : i;
    if (d & 1) gfx_fill(x - a, y - 2 + i, 2 * a + 1, 1, c);
    else gfx_fill(x - 2 + i, y - a, 1, 2 * a + 1, c);
  }
}
/* The calculator's keyboard, the keys of this game in each player's colour: arrows on the keys
   that move, a dot on the one that fires. */
static void keyboard(int x0, int y0) {
  static const int8_t ARROW[4][2] = {{0, 9}, {13, 0}, {13, 18}, {26, 9}}, DIR[4] = {2, 3, 1, 0};
  box(x0 - 4, y0 - 4, 132, 136, 10, HEX(0x0E1124));
  disc(x0 + 62, y0 + 19, 6, HEX(0x3A4268)); /* Home */
  for (int k = 0; k < 53; k++) {
    int x = x0 + 4, y = y0, w = 12, h = 8, p = -1, b = 0;
    if (k < 4) x = x0 + 6 + ARROW[k][0], y = y0 + 6 + ARROW[k][1];
    else if (k < 6) x += 82 + (k - 4) * 16, y += 15;
    else if (k < 12 || (k >= 30 && (k - 30) % 6 == 5)) continue;
    else if (k < 30) x += (k - 12) % 6 * 20, y += 38 + (k - 12) / 6 * 12, w = 16;
    else x += (k - 30) % 6 * 24, y += 74 + (k - 30) / 6 * 14, w = 20, h = 10;
    for (int q = 0; q < 2; q++)
      for (int i = 0; i < 5; i++)
        if (KEYS[q][i] == k && (CONTROL[game] & (1<<i)) && !is_cpu(q))
          p = q, b = i;
    box(x, y, w, h, 3, p < 0 ? HEX(0x3A4268) : team[p]);
    if (p >= 0 && b == 4) disc(x + w / 2, y + h / 2, 2, WHITE);
    else if (p >= 0) tri(x + w / 2, y + h / 2, DIR[b], WHITE);
  }
}

static void button(int x, int y, int w, const char *s, bool on) {
  box(x, y, w, 26, 13, on ? GOLD : HEX(0x3A4372));
  gfx_text_center(&np_font_body, x + w / 2, y + 18, s, on ? INK : WHITE, 256);
}
/* a card over the game: its title, then its buttons from by down. Over a game it is drawn in that game's
   look: Pong's black court and white walls with its square ball, the air hockey rink with a mallet, the tanks' grey maze, the light cycles' blue grid; over the menu, in the menu's */
static void card(int y, int h, const char *title, const char *const *items, int n, int by) {
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, 0, 130);
  if (screen <= S_RULES || (screen == S_ASK && back_to == S_MENU) || game>=QUICK || screen==S_SAVE_ERROR) {
    box(60, y, 200, h, 14, HEX(0x262D52));
    gfx_rrect_outline(60, y, 200, h, 14, 2, HEX(0x56619A), 256);
    say(&np_font_title, 160, y + 30, title, WHITE);
    for (int i = 0; i < n; i++) button(84, by + 32 * i, 152, items[i], sel == i);
    return;
  }
  static const uint32_t LOOK[4][3] = {{0x000000, 0xFFFFFF, 0xFFFFFF}, {0xF4F6FA, 0x9AA3B8, 0x161A33},
                                       {0xE6E6EA, 0x3A3D4F, 0x3A3D4F}, {0x0B1230, 0x3E8BFF, 0xFFFFFF}};
  color_t bg = gfx_rgb(LOOK[game][0]), edge = gfx_rgb(LOOK[game][1]), ink = gfx_rgb(LOOK[game][2]);
  gfx_fill(57, y - 3, 206, h + 6, edge);
  gfx_fill(60, y, 200, h, bg);
  if (game == CYCLES)
    for (int k = 16; k < 200; k += 16) gfx_fill(60 + k, y, 1, h, HEX(0x1C2A55)), k < h ? gfx_fill(60, y + k, 200, 1, HEX(0x1C2A55)) : (void)0;
  say(&np_font_title, 160, y + 30, title, ink);
  for (int i = 0; i < n; i++) {
    int ty = by + 32 * i + 18, x = 160 - gfx_text_width(&np_font_body, items[i]) / 2 - 16;
    gfx_text_center(&np_font_body, 160, ty, items[i], sel == i && game ? team[0] : ink, 256);
    if (sel != i) continue;
    if (game == PONG) gfx_fill(x, ty - 9, 6, 6, WHITE); /* the ball */
    else if (game == HOCKEY) gfx_circle(x + 3, ty - 6, 6, team[0], 256), gfx_circle(x + 3, ty - 6, 3, light[0], 256);
    else gfx_fill(x - 6, ty - 7, 12, 3, team[0]); /* a shot, a trail */
  }
}

/* the little picture of each game in the menu: x, y, w, h, roundness, colour (red, blue, white) */
static const int8_t ICON[][6] = {
  {4, 6, 3, 10, 0, 0}, {19, 10, 3, 10, 0, 1}, {12, 11, 3, 3, 0, 2},                       /* Pong */
  {3, 8, 10, 10, 5, 0}, {14, 6, 10, 10, 5, 1}, {10, 16, 6, 6, 3, 2},                      /* Air Hockey */
  {0, 0, 0, 0, 0, 0},                                                                     /* (a tank) */
  {4, 7, 12, 3, 0, 0}, {13, 7, 3, 12, 0, 0}, {10, 19, 12, 3, 0, 1}, {10, 12, 3, 9, 0, 1}, /* Light Cycles */
};
static const uint8_t ICON_AT[5] = {0, 3, 6, 7, 11};
static void icon(int g, int x, int y) {
  const color_t C[3] = {team[0], team[1], WHITE};
  box(x, y, 26, 26, 6, INK);
  if(g>=QUICK){
    int cx=x+13,cy=y+13;
    switch(g){
      case QUICK:disc(cx-5,cy,5,team[0]);disc(cx+5,cy,5,team[1]);box(cx-2,cy-8,4,16,2,GOLD);break;
      case TUG:gfx_fill(x+3,cy,20,3,WHITE);box(cx-2,cy-5,5,11,2,GOLD);break;
      case FOOTBALL:case VOLLEY:disc(cx,cy,8,g==FOOTBALL?WHITE:GOLD);disc(cx-2,cy-2,3,g==FOOTBALL?INK:WHITE);break;
      case ARCHERY:gfx_fill(cx-8,cy,17,2,WHITE);gfx_fill(cx+5,cy-3,2,8,team[0]);break;
      case DODGE:disc(cx-5,cy+3,5,team[0]);disc(cx+6,cy-5,5,team[1]);break;
      case METEORS:disc(cx+3,cy-4,6,HEX(0xA5AFCB));box(cx-7,cy+6,11,4,2,team[0]);break;
      case ROCKETS:box(cx-8,cy-4,16,9,4,team[1]);gfx_fill(cx-11,cy-2,5,5,GOLD);break;
      case SUMO:disc(cx,cy,10,GOLD);disc(cx-4,cy,4,team[0]);disc(cx+4,cy,4,team[1]);break;
      case FISHING:gfx_fill(cx,cy-9,1,14,WHITE);box(cx-6,cy+3,13,7,3,GOLD);break;
      default:tri(cx,cy-3,3,team[0]);gfx_fill(cx-1,cy-2,3,11,team[1]);break;
    }
  }else if (g == TANKS) tank(x + 13, y + 14, 54, 0);
  else
    for (int i = ICON_AT[g]; i < ICON_AT[g + 1]; i++)
      box(x + ICON[i][0], y + ICON[i][1], ICON[i][2], ICON[i][3], ICON[i][4], C[ICON[i][5]]);
}

static void scene(void *ctx) {
  char s[48];
  if (screen <= S_RULES || (screen == S_ASK && back_to == S_MENU)) {
    gfx_vgrad(0, 0, 160, SCREEN_H, 0xB8323A, 0x3A0F24); /* the two players' colours */
    gfx_vgrad(160, 0, 160, SCREEN_H, 0x2A5FD0, 0x0F1A44);
    say(&np_font_title, 160, 28, T("2 Players"), WHITE);
    char*page_end=num(s,(uint32_t)(game/4+1));*page_end++='/';num(page_end,(NG+3)/4);
    gfx_text_right(&np_font_small,301,28,s,WHITE,256);
    for (int g = game/4*4; g < NG && g < game/4*4+4; g++) {
      int y = 46 + 39 * (g%4), on = g == game;
      gfx_rrect(24, y, 272, 32, 10, on ? WHITE : INK, on ? 256 : 170);
      icon(g, 27, y + 3);
      gfx_text(&np_font_body, 62, y + 21, NAME[g], on ? INK : WHITE, 256);
      for (int p = 0; p < 2; p++) { /* the matches each side has won */
        unsigned wins=sv.wins[g][cpu > 0][p];char*e=num(s,wins>999?999:wins);if(wins>999)*e++='+',*e=0;
        gfx_text_center(&np_font_body, 246 + 36 * p, y + 21, s, team[p], 256);
      }
      gfx_text_center(&np_font_body, 263, y + 21, "-", on ? INK : WHITE, 256);
    }
    gfx_rrect(40, 212, 240, 24, 12, INK, 200);
    tri(52, 224, 2, WHITE), tri(268, 224, 0, WHITE);
    gfx_text_center(&np_font_body, 160, 229, opponent(), WHITE, 256);
    gfx_text_center(&np_font_small,160,207,T("7 / 9: page"),WHITE,256);
    if (screen == S_RULES) {
      gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, 0, 140);
      box(8, 8, 304, 224, 14, HEX(0x262D52));
      say(&np_font_title, 160, 34, NAME[game], WHITE);
      gfx_paragraph(&np_font_small, 160, 52, 284, 11, RULES[game], HEX(0xD6DCF5), 256);
      char *e = s;
      for (const char *q = T("First to "); *q; q++) *e++ = *q;
      num(e, TARGET[game]);
      gfx_text_center(&np_font_body, 231, 184, s, GOLD, 256);
      keyboard(26, 100);
      gfx_text_center(&np_font_small,231,101,CONTROL[game]&16?T("Action: OK / EXE"):T("Use direction keys"),GOLD,256);
      for (int p = 0; p < 2; p++) {
        box(166, 110 + 30 * p, 14, 14, 4, team[p]);
        gfx_text(&np_font_body, 188, 122 + 30 * p, p ? opponent() : who(0), WHITE, 256);
      }
      button(166, 196, 130, T("Play"), true);
    }
  } else {
    PAINT[game]();
    if (game != PONG) { /* the score bar: each side's name and points */
      gfx_fill(0, 0, SCREEN_W, 22, INK);
      for (int p = 0; p < 2; p++) {
        box(p ? 214 : 4, 3, 102, 17, 8, team[p]);
        gfx_text(&np_font_small, p ? 222 : 12, 15, who(p), WHITE, 256);
        num(s, (uint32_t)score[p]);
        gfx_text_right(&np_font_body, p ? 308 : 98, 16, s, WHITE, 256);
      }
    }
    if (phase == 1 && screen == S_PLAY) {
      int t = (int)ago(phase_t), y = 120 - (t < 200 ? (200 - t) / 10 : 0);
      gfx_rrect(70, y - 26, 180, 62, 14, INK, 200);
      if (winner < 2) say(&np_font_title, 160, y, who(winner), light[winner]);
      say(&np_font_title, 160, y + 26, winner < 2 ? "+1" : T("Draw!"), WHITE);
    }
    if (screen == S_PAUSE) {
      static const char *const ITEM[4] = {T("Resume"), T("Restart"), T("Other games"), T("Quit game")};
      card(40, 172, T("Paused"), ITEM, 4, 82);
    } else if (screen == S_OVER) {
      static const char *const WON[2][2] = {{T("Player 1 wins!"), T("Player 2 wins!")}, {T("You win!"), T("The computer wins")}};
      static const char *const ITEM[2] = {T("Rematch"), T("Other games")};
      card(46, 158, WON[cpu > 0][score[1] > score[0]], ITEM, 2, 140);
      for (int p = 0; p < 2; p++) {
        num(s, (uint32_t)score[p]);
        say(&np_font_title, 124 + 72 * p, 118, s, light[p]);
      }
      say(&np_font_title, 160, 118, "-", WHITE);
    }
  }
  if (screen == S_ASK) {
    static const char *const ITEM[2] = {T("No"), T("Yes")};
    card(60, 120, T("Quit game?"), ITEM, 2, 104);
  }else if(screen==S_SAVE_ERROR){
    static const char*const ITEM[2]={T("Retry saving"),T("Exit without saving")};
    card(45,158,T("Save failed"),ITEM,2,113);
  }
}

int main(void) {
  np_app_begin();
  color_t *their_strip=gfx_buf;gfx_buf=strip;
  static const uint32_t TEAM[2]={0xEF4136,0x2E7CF6};
  for(int p=0;p<2;p++)team[p]=gfx_rgb(TEAM[p]),light[p]=gfx_rgb(gfx_lerp888(TEAM[p],0xFFFFFF,120)),dark[p]=gfx_rgb(gfx_lerp888(TEAM[p],0,110));
  uint32_t last=(uint32_t)eadk_timing_millis(),n=0,accum=0,draw_at=last;
  seed=eadk_random()^last;if(!seed)seed=1;
  const uint8_t*d=np_file_read(SAVE_NAME,&n);save_decode(d,n);
  game=sv.game%NG;cpu=sv.cpu&3;screen=S_MENU;sel=0;now=0;
  uint64_t held=eadk_keyboard_scan(),stale=held;int previous_pad[2]={0},queued[2]={0};
  bool dirty=true;
  for(bool quit=false;!quit;){
    uint32_t t=(uint32_t)eadk_timing_millis(),elapsed=t-last;last=t;
    uint64_t k=eadk_keyboard_scan();stale&=k;k&=~stale;uint64_t hit=k&~held;held=k;
    bool ok=down(hit,eadk_key_ok)||down(hit,eadk_key_exe),back=down(hit,eadk_key_back);
    int dx=down(hit,eadk_key_right)-down(hit,eadk_key_left),dy=down(hit,eadk_key_down)-down(hit,eadk_key_up),before=screen;
    if(hit)dirty=true;
    if(screen!=S_SAVE_ERROR&&(down(k,eadk_key_home)||down(k,eadk_key_on_off))){
      if(save())break;
      screen=S_SAVE_ERROR;sel=0;stale|=k;ok=back=false;dx=dy=0;dirty=true;
    }
    pad[0]=pad[1]=0;
    for(int i=0;i<10;i++)pad[i/5]|=down(k,KEYS[i/5][i%5])<<(i%5);
    if(cpu&&(down(k,eadk_key_ok)||down(k,eadk_key_exe)))pad[0]|=B_FIRE;
    for(int p=0;p<2;p++){tap[p]=pad[p]&~previous_pad[p];previous_pad[p]=pad[p];queued[p]|=tap[p];}
    switch(screen){
      case S_MENU:
      case S_RULES:
        if(dx){cpu=(cpu+dx+4)%4;save();}
        if(screen==S_RULES){if(ok)new_match();else if(back)screen=S_MENU;}
        else if(dy)game=(game+dy+NG)%NG;
        else if(down(hit,eadk_key_seven))game=page_game(-1);
        else if(down(hit,eadk_key_nine))game=page_game(1);
        else if(ok){screen=S_RULES;save();}
        else if(back){back_to=S_MENU;screen=S_ASK;sel=0;}
        break;
      case S_PLAY:
        if(back){screen=S_PAUSE;sel=0;break;}
        /* A fixed eight-millisecond simulation, independent of LCD frame rate.
         * A tap is queued until a tick consumes it; held keys keep moving. */
        accum+=elapsed>160?160:elapsed;
        for(int steps=0;accum>=8&&steps<20&&screen==S_PLAY;steps++){
          ms=8;now+=8;tap[0]=queued[0];tap[1]=queued[1];queued[0]=queued[1]=0;
          match_step();accum-=8;
        }
        break;
      case S_SAVE_ERROR:
        if(dy)sel^=1;
        if(back||(ok&&sel))quit=true;else if(ok&&save())quit=true;
        break;
      default:
        sel=(sel+dy)&(screen==S_PAUSE?3:1);
        if(screen==S_ASK){
          if(back||(ok&&!sel))screen=back_to;
          else if(ok){if(save())quit=true;else screen=S_SAVE_ERROR,sel=0;}
        }else if(back&&screen==S_PAUSE)screen=S_PLAY;
        else if(ok){
          int a=screen==S_PAUSE?sel:sel?2:1;
          if(a==1)new_match();
          else{screen=a?(a==2?S_MENU:S_ASK):S_PLAY;back_to=S_PAUSE;sel=0;}
        }
        break;
    }
    if(screen!=S_PLAY)accum=0,queued[0]=queued[1]=0;
    if(before!=screen){
      dirty=true;accum=0;queued[0]=queued[1]=0;previous_pad[0]=previous_pad[1]=0;stale|=k;
      last=(uint32_t)eadk_timing_millis();
    }
    if(dirty||(screen==S_PLAY&&t-draw_at>=30)){
      eadk_display_wait_for_vblank();gfx_render(scene,0,0,SCREEN_H);draw_at=t;dirty=false;
    }
    eadk_timing_msleep(2);
  }
  gfx_buf=their_strip;
  return np_app_end();
}
