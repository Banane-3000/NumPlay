/* The objects' code, by file (each object is the original's object of that id), and what they share. */
#ifndef S1_OBJS_H
#define S1_OBJS_H
#include "s1.h"

/* tile word flags */
#define Tile_Prio 0x8000
#define Tile_Pal2 0x2000
#define Tile_Pal3 0x4000
#define Tile_Pal4 0x6000

/* the zone and act as one word, as the original's v_zone_act */
#define ZONE_ACT ((uint16_t)(V.zone << 8 | V.act))
static inline void set_zone_act(uint16_t za) { V.zone = (uint8_t)(za >> 8), V.act = (uint8_t)za; }

/* obTimeFrame and obDelayAni read together as a word */
static inline uint16_t timew(const Obj *o) { return (uint16_t)(o->timeframe << 8 | o->delayani); }
static inline void set_timew(Obj *o, uint16_t v) { o->timeframe = (uint8_t)(v >> 8), o->delayani = (uint8_t)v; }

/* the high and low bytes of an object's own word (move.b of a word's address reads its high byte) */
#define OWH(o, off) ((uint8_t)(OUW(o, off) >> 8))

/* what touching an object does (obColType): its hitbox, and its kind */
enum {
  col_none = 0, col_badnik = 0, col_boss = 0, col_item = 0x40, col_hurt = 0x80, col_special = 0xC0,
  col_40x40 = 1, col_24x40, col_40x24, col_8x32, col_24x36, col_32x32, col_12x12, col_48x24, col_24x32,
  col_32x24, col_16x16, col_40x32, col_40x16, col_28x28, col_48x48, col_80x32, col_32x48, col_16x32,
  col_64x224, col_128x64, col_256x64, col_64x64, col_16x16_alt, col_8x8, col_64x16, col_24x24, col_16x8,
  col_48x8, col_80x8, col_8x16, col_8x48, col_8x80, col_8x64, col_48x48_alt, col_24x48, col_144x16
};

/* a boss's base position (longs at $30 and $38), its hits left and flash time */
#define OFIX(o, off) (*(fix_t *)(void *)&(o)->s.b[(off) - 0x28])
#define obBossX(o) OFIX(o, 0x30)
#define obBossY(o) OFIX(o, 0x38)
#define obBossHits(o) ((o)->colprop)
#define obBossFlash 0x3E

#define vblank_byte ((uint8_t)V.vblank_count)

/* v_oscillate's bytes and words by their offset from it (2: the first value) */
#define OSC_B(off) ((uint8_t)(V.osc[((off) - 2) >> 1] >> (((off) & 1) ? 0 : 8)))
#define OSC_W(off) (V.osc[((off) - 2) >> 1])

/* objects that keep a list of their children's slots after a count (at the subtype) */
#define CHILD(o, i) OBJ((o)->s.b[1 + (i)] & 0x7F)
#define SLOT(o) ((uint8_t)(((o) - objs) & 0x7F))
/* obAngle read as a word (with the byte after it) */
static inline uint16_t anglew(const Obj *o) { return (uint16_t)(o->angle << 8 | o->x27); }
static inline void set_anglew(Obj *o, uint16_t v) { o->angle = (uint8_t)(v >> 8), o->x27 = (uint8_t)v; }

/* Sonic's own variables */
#define flashtime 0x30
#define invtime 0x32
#define shoetime 0x34
#define angleright 0x36
#define angleleft 0x37
#define sticktoconvex 0x38
#define restartime 0x3A
#define jumping 0x3C
#define standonobject 0x3D
#define locktime 0x3E
#define sonic_solid_width 11

/* the animation of Sonic's ids (id_Walk, id_Roll...) are in s1const.h */

/* cards.c */
void TitleCard(Obj *o);
void GameOverCard(Obj *o);
void GotThroughCard(Obj *o);
void Signpost(Obj *o);
void GotThroughAct(void);
void Lamppost(Obj *o);
void Lamp_LoadInfo(void);

/* items.c, hud.c */
void Rings(Obj *o);
void RingLoss(Obj *o);
void Monitor(Obj *o);
void PowerUp(Obj *o);
void ExplosionItem(Obj *o);
void Explosion(Obj *o);
void Animals(Obj *o);
void Points(Obj *o);
void HUD(Obj *o);
void ContScrCounter(uint32_t v);

/* common.c */
void ShieldItem(Obj *o);
void Springs(Obj *o);
void Spikes(Obj *o);
void GiantRing(Obj *o);
void RingFlash(Obj *o);
void HiddenBonus(Obj *o);
void Prison(Obj *o);
void SmashObject(Obj *o, const int16_t *speeds, int n, int16_t gravity);
void BossDefeated(Obj *o);
void BossMove(Obj *o);
void Invisibarrier(Obj *o);
void Button(Obj *o);
void PushBlock(Obj *o);
void WaterSound(Obj *o);
void EdgeWalls(Obj *o);

/* obj_ghz.c */
void Bridge(Obj *o);
void SwingingPlatform(Obj *o);
void Helix(Obj *o);
void BasicPlatform(Obj *o);
void CollapseLedge(Obj *o);
void CollapseFloor(Obj *o);
void Scenery(Obj *o);
void PurpleRock(Obj *o);
void SmashWall(Obj *o);
void MotoBug(Obj *o);
void Crabmeat(Obj *o);
void BuzzBomber(Obj *o);
void Missile(Obj *o);
void UnusedExplosion(Obj *o);
void Chopper(Obj *o);
void Newtron(Obj *o);
void BossGreenHill(Obj *o);
void BossBall(Obj *o);
void boss_hit_flash(Obj *o, uint8_t restore);

/* screens.c */
void TitleSonic(Obj *o);
void PSBTM(Obj *o);
void CreditsText(Obj *o);
#endif
