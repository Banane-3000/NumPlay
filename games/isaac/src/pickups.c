/* What lies about a room: pickups and chests, items on their pedestals,
 * shops, machines and beggars, bombs, effects, blood and creep. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

static const int16_t pk_sprite[PK_COUNT] = {
  [PK_HEART] = SPR_HEART_FULL, [PK_HALF_HEART] = SPR_HEART_HALF, [PK_SOUL] = SPR_HEART_SOUL, [PK_BLACK] = SPR_HEART_BLACK,
  [PK_ETERNAL] = SPR_HEART_ETERNAL, [PK_DOUBLE_HEART] = SPR_HEART_DOUBLE, [PK_HALF_SOUL] = SPR_HEART_HALFSOUL,
  [PK_PENNY] = SPR_PENNY, [PK_NICKEL] = SPR_NICKEL, [PK_DIME] = SPR_DIME, [PK_KEY] = SPR_KEY, [PK_GOLDEN_KEY] = SPR_GOLDEN_KEY,
  [PK_KEY_RING] = SPR_KEY_RING, [PK_BOMB] = SPR_BOMB_PICKUP, [PK_DOUBLE_BOMB] = SPR_DOUBLE_BOMB, [PK_TROLL_BOMB] = SPR_TROLL_BOMB,
  [PK_CHEST] = SPR_CHEST, [PK_LOCKED_CHEST] = SPR_LOCKED_CHEST, [PK_RED_CHEST] = SPR_RED_CHEST, [PK_PILL] = SPR_PILL,
  [PK_CARD] = SPR_CARD, [PK_BATTERY] = SPR_BATTERY, [PK_GRAB_BAG] = SPR_GRAB_BAG, [PK_SPIKED_CHEST] = SPR_SPIKED_CHEST,
  [PK_GOLDEN_BOMB] = SPR_GOLDEN_BOMB,
};

Ent *pickup_spawn(int type, int sub, float x, float y, bool pop) {
  if (type == PK_TROLL_BOMB) {   /* a troll bomb is lit as it lands */
    bomb_place(x, y, 1);
    return NULL;
  }
  Ent *e = ent_new(EK_PICKUP, x, y);
  if (!e) return NULL;
  e->type = (uint8_t)type, e->r = 6;
  if (type == PK_PILL && !sub) sub = 1 + rnd(13);
  if (type == PK_CARD && !sub) sub = rnd(6) == 0 ? 23 + rnd(9) : 1 + rnd(22);
  e->sub = (uint8_t)sub;
  e->spr = pk_sprite[type];
  e->frame = type == PK_PILL ? sub - 1 : type == PK_CARD ? sub - 1 : 0;
  if (pop) {
    float a = rndf() * 6.2831853f, s = 1 + rndf() * 2;
    e->vx = fcos(a) * s, e->vy = fsin(a) * s, e->vz = -3, e->z = -2;
  }
  e->t = 10;   /* (it can't be picked up the moment it appears) */
  return e;
}

/* a random pickup, as rooms and rocks drop them */
int pickup_random_type(void) {
  int r = rnd(100);
  if (r < 30) return rnd(10) == 0 ? PK_NICKEL : rnd(30) == 0 ? PK_DIME : PK_PENNY;
  if (r < 50) return rnd(4) == 0 ? PK_HALF_HEART : rnd(8) == 0 ? PK_SOUL : rnd(25) == 0 ? PK_BLACK : PK_HEART;
  if (r < 64) return rnd(15) == 0 ? PK_KEY_RING : rnd(40) == 0 ? PK_GOLDEN_KEY : PK_KEY;
  if (r < 78) return rnd(8) == 0 ? PK_DOUBLE_BOMB : rnd(30) == 0 ? PK_TROLL_BOMB : PK_BOMB;
  if (r < 86) return rnd(6) == 0 ? PK_LOCKED_CHEST : PK_CHEST;
  if (r < 93) return PK_PILL;
  if (r < 99) return PK_CARD;
  return PK_BATTERY;
}

void pickup_drop_random(float x, float y) { pickup_spawn(pickup_random_type(), 0, x, y, true); }

/* the room's reward when it is cleared: one time in two or so, better with luck */
void room_clear_reward(void) {
  Room *r = room_cur();
  if (r->kind == ROOM_BOSS || r->kind == ROOM_MINIBOSS) return;
  float luck = pl.luck;
  int roll = rnd(100) + (int)(luck * 4);
  if (roll < 45) return;
  /* where: the room's middle, or the nearest free cell */
  float x = 160, y = 120;
  for (int k = 0; k < 20 && solid_at(x, y, false); k++) x = 60 + rnd(200), y = 70 + rnd(100);
  pickup_drop_random(x, y);
}

static void collect_coins(int n) {
  int c = pl.coins + n;
  pl.coins = (uint8_t)(c > 99 ? 99 : c);
  if (pl.has[109]) player_stats();
}

/* opening a chest: what spills out */
static void open_chest(Ent *e) {
  int t = e->type;
  e->state = 1;   /* (open, empty) */
  e->frame = 0;
  if (t == PK_SPIKED_CHEST) player_hurt(1, -6);
  if (t == PK_RED_CHEST) {
    int r = rnd(100);
    if (r < 10 && fl.stage > 1) pedestal_spawn(item_from_pool(POOL_DEVIL), e->x, e->y, 0, false);
    else if (r < 35) for (int i = 0; i < 2; i++) pickup_spawn(PK_SOUL, 0, e->x, e->y, true);
    else if (r < 50) pickup_spawn(PK_BLACK, 0, e->x, e->y, true);
    else if (r < 65) for (int i = 0; i < 2; i++) pickup_spawn(PK_PILL, 0, e->x, e->y, true);
    else if (r < 80) for (int i = 0; i < 3; i++) mon_spawn(MON_SPIDER, e->x, e->y);
    else for (int i = 0; i < 2; i++) pickup_spawn(PK_TROLL_BOMB, 0, e->x, e->y, true);
    e->kind = EK_NONE;
    return;
  }
  bool golden = t == PK_LOCKED_CHEST;
  if (golden && rnd(5) == 0) {
    pedestal_spawn(item_from_pool(POOL_TREASURE), e->x, e->y, 0, false);
    e->kind = EK_NONE;
    return;
  }
  int n = golden ? 2 + rnd(4) : 1 + rnd(3);
  for (int i = 0; i < n; i++) {
    int k = rnd(golden ? 8 : 6);
    static const uint8_t brown[6] = {PK_PENNY, PK_PENNY, PK_HEART, PK_KEY, PK_BOMB, PK_PILL};
    static const uint8_t gold[8] = {PK_PENNY, PK_NICKEL, PK_SOUL, PK_KEY, PK_BOMB, PK_PILL, PK_CARD, PK_PENNY};
    pickup_spawn(golden ? gold[k] : brown[k], 0, e->x, e->y, true);
  }
  if (rnd(golden ? 10 : 20) == 0) pickup_spawn(PK_CARD, 0, e->x, e->y, true);
  e->kind = EK_NONE;
}

static bool try_collect(Ent *e) {
  int t = e->type;
  if (e->flags & EF_SHOP) {   /* for sale */
    if (pl.coins < e->price) return false;
  }
  int max = pl.containers * 2;
  switch (t) {
    case PK_HEART: case PK_HALF_HEART: case PK_DOUBLE_HEART:
      if (pl.red >= max) return false;
      player_heal(t == PK_HEART ? 2 : t == PK_HALF_HEART ? 1 : 4);
      break;
    case PK_SOUL: case PK_HALF_SOUL: case PK_BLACK:
      if (pl.containers * 2 + pl.soul >= 24) return false;
      player_add_soul(t == PK_HALF_SOUL ? 1 : 2, t == PK_BLACK);
      break;
    case PK_ETERNAL:
      if (pl.eternal) player_add_containers(1), player_heal(2), pl.eternal = 0;
      else pl.eternal = 1;
      break;
    case PK_PENNY: collect_coins(1); break;
    case PK_NICKEL: collect_coins(5); break;
    case PK_DIME: collect_coins(10); break;
    case PK_KEY: if (pl.keys < 99) pl.keys++; break;
    case PK_KEY_RING: pl.keys = (uint8_t)(pl.keys + 2 > 99 ? 99 : pl.keys + 2); break;
    case PK_GOLDEN_KEY: pl.keys = (uint8_t)(pl.keys + 1 > 99 ? 99 : pl.keys + 1), pl.has[175] |= 0; break;
    case PK_BOMB: if (pl.bombs < 99) pl.bombs++; break;
    case PK_DOUBLE_BOMB: pl.bombs = (uint8_t)(pl.bombs + 2 > 99 ? 99 : pl.bombs + 2); break;
    case PK_GOLDEN_BOMB: pl.bombs = (uint8_t)(pl.bombs + 1 > 99 ? 99 : pl.bombs + 1); break;
    case PK_BATTERY: {
      const ItemInfo *ii = item_info(pl.active);
      if (!ii || !ii->charge || ii->charge == 255 || pl.active_charge >= ii->charge) return false;
      pl.active_charge = ii->charge;
      break;
    }
    case PK_PILL: case PK_CARD: {
      if (pl.pocket_kind) {   /* (the one he had drops where he stands) */
        Ent *o = pickup_spawn(pl.pocket_kind == 1 ? PK_PILL : PK_CARD, pl.pocket, pl.x, pl.y + 8, false);
        if (o) o->t = 40;
      }
      pl.pocket_kind = t == PK_PILL ? 1 : 2, pl.pocket = e->sub;
      hud_banner(t == PK_PILL ? pill_name(e->sub, pl.pill_known[e->sub] != 0) : card_name(e->sub), "");
      break;
    }
    case PK_GRAB_BAG:
      for (int i = 0; i < 2; i++) pickup_spawn(rnd(3) == 0 ? PK_KEY : rnd(2) ? PK_PENNY : PK_BOMB, 0, e->x, e->y, true);
      break;
    case PK_CHEST: case PK_SPIKED_CHEST: case PK_RED_CHEST:
      open_chest(e);
      return true;
    case PK_LOCKED_CHEST:
      if (!pl.keys && !pl.has[175]) return false;
      if (!pl.has[175]) pl.keys--;
      open_chest(e);
      return true;
    default: break;
  }
  if (e->flags & EF_SHOP) pl.coins = (uint8_t)(pl.coins - e->price), player_stats();
  fx_spawn(FX_SPARK, e->x, e->y - 6, 8);
  e->kind = EK_NONE;
  return true;
}

void pickup_update(Ent *e) {
  if (e->t > 0) e->t--;
  /* falling and bouncing */
  if (e->z < 0 || e->vz) {
    e->vz += 0.6f, e->z += e->vz;
    if (e->z >= 0) {
      e->z = 0;
      e->vz = e->vz > 1.5f ? -e->vz * 0.4f : 0;
    }
  }
  /* Magneto pulls coins, hearts and keys */
  if (pl.has[53] && e->type <= PK_GOLDEN_BOMB && !(e->flags & EF_SHOP) && e->type != PK_CHEST && e->type != PK_LOCKED_CHEST) {
    float dx = pl.x - e->x, dy = pl.y - e->y, d = fsqrt(dx * dx + dy * dy);
    if (d > 1 && d < 120) e->vx += dx / d * 0.25f, e->vy += dy / d * 0.25f;
  }
  e->vx *= 0.88f, e->vy *= 0.88f;
  move_ent(&e->x, &e->y, &e->vx, &e->vy, 5, false, false);
  if (e->t > 0 || e->z < -4) return;
  float dx = pl.x - e->x, dy = pl.y - e->y;
  if (dx * dx + dy * dy < 11 * 11) {
    if (!try_collect(e)) {
      /* not taken: nudged aside (chests, hearts he doesn't need) */
      if (!(e->flags & EF_SHOP) && e->type != PK_LOCKED_CHEST) {
        float d = fsqrt(dx * dx + dy * dy);
        if (d > 0.5f) e->vx -= dx / d * 0.8f, e->vy -= dy / d * 0.8f;
      }
    }
  }
}

void pickup_draw(Ent *e) {
  int x = (int)e->x, y = (int)(e->y + e->z);
  if (e->kind == EK_BOMB) {
    int a = e->t < 15 && (e->t & 2) ? 20 : 0;
    int s = 256 + (e->t < 15 ? ((e->t & 3) * 12) : 0);
    gfx_sprite_scaled(e->spr, 0, x, y + 3, s, s, 0, C_RED, a, 32);
    return;
  }
  gfx_sprite(e->spr, e->frame, x, y + 4, 0, 32);
  if (e->flags & EF_SHOP) {   /* its price under it */
    char s[8];
    int p = e->price, n = 0;
    if (p >= 10) s[n++] = (char)('0' + p / 10);
    s[n++] = (char)('0' + p % 10), s[n] = 0;
    gfx_text_c(FONT_UPHEAVAL, x, y + 8, s, C_WHITE, 1, true);
  }
}

/* ---------------------------------------------------------------- items on pedestals */
Ent *pedestal_spawn(int item, float x, float y, int price, bool devil) {
  Ent *e = ent_new(EK_PEDESTAL, x, y);
  if (!e) return NULL;
  e->t3 = (int16_t)item, e->price = (int16_t)price, e->r = 8;
  if (devil) e->flags |= EF_DEVIL;
  e->t = 15;
  return e;
}

/* what an item costs in the Devil's room: a red heart container, two for the best, or three soul hearts */
static int devil_price(int item) {
  const ItemInfo *ii = item_info(item);
  return ii && ii->quality >= 3 ? 2 : 1;
}

void pedestal_update(Ent *e) {
  if (e->t > 0) e->t--;
  if (!e->t3 || e->t > 0) return;
  float dx = pl.x - e->x, dy = pl.y - e->y;
  if (dx * dx + dy * dy > 11 * 11) return;
  if (e->price && !(e->flags & EF_DEVIL)) {   /* the shop */
    if (pl.coins < e->price) return;
    pl.coins = (uint8_t)(pl.coins - e->price);
  }
  if (e->flags & EF_DEVIL) {
    int hp = devil_price(e->t3);
    if (pl.containers >= hp) player_add_containers(-hp);
    else if (pl.soul >= 6 && pl.containers == 0) pl.soul = (uint8_t)(pl.soul - 6);
    else if (pl.containers > 0) player_add_containers(-pl.containers);
    else return;
    if (!pl.containers && !pl.soul) player_kill(-7);
  }
  int item = e->t3;
  e->t3 = 0, e->price = 0;
  e->flags &= (uint16_t)~EF_DEVIL;
  /* an active item swaps with the one he holds */
  if (item_is_active(item) && pl.active) {
    e->t3 = pl.active, e->t = 30;
  }
  item_give(item);
  pl.hold_anim = 30, pl.hold_item = (int16_t)item;
}

void pedestal_draw(Ent *e) {
  int x = (int)e->x, y = (int)e->y;
  gfx_sprite(SPR_PEDESTAL, 0, x, y + 6, 0, 32);
  if (e->t3) {
    int bob = (int)(g_frame / 4 + e->x) % 8;
    bob = bob < 4 ? bob : 8 - bob;
    int spr = item_sprite(e->t3);
    if ((fl.curse & CURSE_BLIND) && !e->price && room_cur()->kind != ROOM_SHOP) spr = SPR_ITEM_1, gfx_text_c(FONT_UPHEAVAL, x, y - 22, "?", C_WHITE, 2, true);
    else gfx_sprite(spr, 0, x, y - 4 - bob, 0, 32);
    if (e->price) {
      char s[8];
      int n = 0, p = e->flags & EF_DEVIL ? devil_price(e->t3) : e->price;
      if (p >= 10) s[n++] = (char)('0' + p / 10);
      s[n++] = (char)('0' + p % 10), s[n] = 0;
      if (e->flags & EF_DEVIL) {
        for (int k = 0; k < p; k++) gfx_sprite(SPR_HUD_HEART, 0, x - p * 6 + k * 12, y + 8, 0, 32);
      } else
        gfx_text_c(FONT_UPHEAVAL, x, y + 9, s, C_WHITE, 1, true);
    }
  }
}

/* ---------------------------------------------------------------- machines and beggars */
static const int16_t ma_sprite[] = {SPR_SLOT_MACHINE, SPR_BLOOD_MACHINE, SPR_FORTUNE_MACHINE, SPR_BEGGAR, SPR_DEVIL_BEGGAR,
                                    SPR_KEY_MASTER, SPR_BOMB_BUM, SPR_SHOPKEEPER, SPR_SATAN, SPR_GABRIEL};

Ent *machine_spawn(int type, float x, float y) {
  Ent *e = ent_new(EK_MACHINE, x, y);
  if (!e) return NULL;
  e->type = (uint8_t)type, e->spr = ma_sprite[type], e->r = 12;
  return e;
}

static const char *const fortunes[] = {
  "YOU WILL FIND\nWHAT YOU SEEK", "BEWARE THE\nHUNGRY ONE", "FRIENDS ARE\nNEARBY", "A SECRET LIES\nWHERE ROOMS MEET",
  "YOUR LUCK\nWILL CHANGE", "DON'T EAT THE\nYELLOW SNOW", "MOM IS\nWATCHING", "THE FLY IS\nYOUR FRIEND",
};

static void machine_pay(Ent *e) {
  e->t2 = 12;   /* (its little shake) */
  switch (e->type) {
    case MA_SLOT: {   /* a coin: mostly nothing, or a pickup, a fly, sometimes a prize */
      if (!pl.coins) return;
      pl.coins--;
      int r = rnd(100) - (int)pl.luck * 2;
      if (r < 5) for (int i = 0; i < 2; i++) pickup_spawn(PK_PENNY, 0, e->x, e->y + 14, true);
      else if (r < 10) pickup_spawn(PK_HEART, 0, e->x, e->y + 14, true);
      else if (r < 15) pickup_spawn(PK_KEY, 0, e->x, e->y + 14, true);
      else if (r < 20) pickup_spawn(PK_BOMB, 0, e->x, e->y + 14, true);
      else if (r < 23) pickup_spawn(PK_PILL, 0, e->x, e->y + 14, true);
      else if (r < 27) mon_spawn(MON_FLY, e->x, e->y + 16);
      else if (r < 29) for (int i = 0; i < 6; i++) pickup_spawn(PK_PENNY, 0, e->x, e->y + 14, true);
      else if (r < 30 && rnd(3) == 0) pedestal_spawn(item_from_pool(POOL_TREASURE), e->x, e->y + 26, 0, false);
      break;
    }
    case MA_BLOOD:   /* half a heart: coins */
      if (!player_hurt(1, -5)) return;
      pl.invuln = 10;
      for (int i = 0; i < 1 + rnd(2); i++) pickup_spawn(PK_PENNY, 0, e->x, e->y + 14, true);
      if (rnd(40) == 0) pickup_spawn(PK_NICKEL, 0, e->x, e->y + 14, true);
      break;
    case MA_FORTUNE: {
      if (!pl.coins) return;
      pl.coins--;
      int r = rnd(100);
      if (r < 4) pedestal_spawn(item_from_pool(POOL_TREASURE), e->x, e->y + 26, 0, false);
      else if (r < 12) pickup_spawn(PK_SOUL, 0, e->x, e->y + 14, true);
      else if (r < 22) pickup_spawn(PK_CARD, 0, e->x, e->y + 14, true);
      else hud_banner(fortunes[rnd(sizeof fortunes / sizeof fortunes[0])], "");
      break;
    }
    case MA_BEGGAR:   /* a coin: a pickup some of the time, an item once he has had enough */
      if (!pl.coins) return;
      pl.coins--;
      e->sub++;
      if (e->sub >= 6 + rnd(6) && rnd(4) == 0) {
        pedestal_spawn(item_from_pool(POOL_SHOP), e->x, e->y + 26, 0, false);
        e->state = 255, e->kind = EK_NONE;
        fx_spawn(FX_POOF, e->x, e->y, 10);
      } else if (rnd(4) == 0)
        pickup_drop_random(e->x, e->y + 14);
      break;
    case MA_DEVIL_BEGGAR:
      if (!player_hurt(2, -5)) return;
      if (rnd(3) == 0) pedestal_spawn(item_from_pool(POOL_DEVIL), e->x, e->y + 26, 0, false), e->state = 255, e->kind = EK_NONE;
      else pickup_spawn(PK_SOUL, 0, e->x, e->y + 14, true);
      break;
    case MA_BOMB_BUM:
      if (!pl.bombs) return;
      pl.bombs--;
      if (rnd(3) == 0) pickup_spawn(rnd(2) ? PK_NICKEL : PK_HEART, 0, e->x, e->y + 14, true);
      break;
    case MA_KEY_MASTER:
      if (!pl.keys) return;
      pl.keys--;
      pickup_spawn(rnd(3) ? PK_CHEST : PK_LOCKED_CHEST, 0, e->x, e->y + 16, true);
      break;
    default: break;
  }
}

void machine_update(Ent *e) {
  if (e->t2 > 0) e->t2--;
  if (e->t > 0) {
    e->t--;
    return;
  }
  if (e->type >= MA_SHOPKEEPER) return;
  float dx = pl.x - e->x, dy = pl.y - e->y;
  if (dx * dx + dy * dy < 15 * 15) {
    machine_pay(e);
    e->t = 15;
    /* he bumps off it */
    float d = fsqrt(dx * dx + dy * dy);
    if (d > 0.5f) pl.vx += dx / d * 2, pl.vy += dy / d * 2;
  }
}

void machine_draw(Ent *e) {
  int sh = e->t2 ? ((e->t2 & 2) ? 1 : -1) : 0;
  if (e->type == MA_DEVIL_STATUE || e->type == MA_ANGEL_STATUE) {
    gfx_sprite_tint(e->spr, 0, (int)e->x, (int)e->y, 0, rgb565(0x707070), 18, 32);
    return;
  }
  gfx_sprite(e->spr, 0, (int)e->x + sh, (int)e->y + 8, 0, 32);
}

/* ---------------------------------------------------------------- bombs */
void bomb_update(Ent *e) {
  /* pushed about by Isaac, slowing down */
  float dx = e->x - pl.x, dy = e->y - pl.y;
  if (dx * dx + dy * dy < 10 * 10 && e->type != 3) {
    float d = fsqrt(dx * dx + dy * dy);
    if (d > 0.5f) e->vx += dx / d * 0.6f, e->vy += dy / d * 0.6f;
  }
  if (e->type == 3) {   /* Dr. Fetus: it flies, and goes off on what it hits */
    Ent *m = ent_nearest_monster(e->x, e->y, 14);
    if (m || e->t <= 0) e->t = 0;
  } else
    e->vx *= 0.85f, e->vy *= 0.85f;
  move_ent(&e->x, &e->y, &e->vx, &e->vy, 5, false, false);
  if (--e->t <= 0) {
    bool mega = e->spr == SPR_BIG_BOMB;
    explode(e->x, e->y, mega ? 52 : 36, e->hp > 0 ? e->hp : 60, e->type != 3, e->spr == SPR_POISON_BOMB ? 2 : 0);
    e->kind = EK_NONE;
  }
}

/* ---------------------------------------------------------------- effects, blood, creep */
void fx_spawn(int type, float x, float y, int life) {
  Ent *e = ent_new(EK_FX, x, y);
  if (!e) return;
  e->type = (uint8_t)type, e->t = (int16_t)life, e->t2 = (int16_t)life;
  if (type == FX_GIB) {
    float a = rndf() * 6.2831853f, s = 1 + rndf() * 2.5f;
    e->vx = fcos(a) * s, e->vy = fsin(a) * s * 0.6f, e->vz = -2 - rndf() * 3, e->z = -6, e->a = 1 + rndf() * 2;
  }
}

Ent *fx_text(float x, float y, const char *s) {
  Ent *e = ent_new(EK_FX, x, y);
  if (!e) return NULL;
  e->type = FX_TEXT, e->t = e->t2 = 40;
  e->spr = 0;
  /* (the text: 1UP, the only one) */
  (void)s;
  return e;
}

void fx_update(Ent *e) {
  if (e->type == FX_SPARK + 100) return;   /* (Mom's Knife) */
  if (e->type == FX_GIB) {
    e->x += e->vx, e->y += e->vy, e->vz += 0.5f, e->z += e->vz;
    if (e->z > 0) {
      e->z = 0, e->vx = e->vy = 0;
      if (e->vz > 0) decal_blood(e->x, e->y, 1.5f + e->a), e->vz = 0;
    }
  }
  if (e->type == FX_TEXT) e->y -= 0.5f;
  if (--e->t <= 0) e->kind = EK_NONE;
}

void fx_draw(Ent *e) {
  int x = (int)e->x, y = (int)e->y;
  int k = e->t2 - e->t;   /* frames since it began */
  switch (e->type) {
    case FX_POOF: {
      int s = 120 + k * 22;
      gfx_sprite_scaled(SPR_SMOKE, 0, x, y + 6, s, s, 0, 0, 0, 24 - k * 2 > 0 ? 24 - k * 2 : 0);
      break;
    }
    case FX_SPLASH:
      gfx_ellipse(x, y, 2 + k, 1 + k / 2, rgb565(0xA0C8F0), 20 - k * 3);
      break;
    case FX_SPARK:
      gfx_ellipse(x, y - k, 3 + k, 3 + k, C_WHITE, 20 - k * 2);
      break;
    case FX_GIB:
      gfx_ellipse(x, (int)(y + e->z), (int)e->a + 1, (int)e->a, rgb565(0x9A1010), 32);
      break;
    case FX_EXPLOSION: {
      float r = e->a * (0.5f + k * 0.12f);
      if (r > e->a * 1.2f) r = e->a * 1.2f;
      int a = 32 - k * 2;
      gfx_ellipse(x, y - 8, (int)r, (int)(r * 0.8f), rgb565(0x302020), a / 2);
      gfx_ellipse(x, y - 8, (int)(r * 0.8f), (int)(r * 0.65f), rgb565(0xF07020), a);
      gfx_ellipse(x, y - 10, (int)(r * 0.5f), (int)(r * 0.4f), rgb565(0xFFE070), a);
      break;
    }
    case FX_TEXT:
      gfx_text_c(FONT_UPHEAVAL, x, y, "1UP!", C_WHITE, 1, true);
      break;
    case FX_SPARK + 100:   /* Mom's Knife */
      gfx_sprite(SPR_KNIFE, 0, x, y, 0, 32);
      break;
    case FX_SHOCKWAVE:
      gfx_ellipse(x, y, k * 4, k * 2, rgb565(0x504030), 20 - k);
      break;
    case FX_LIGHT:
      gfx_rect(x - 10, 0, 20, y, C_WHITE, e->t < 8 ? e->t * 3 : 24);
      break;
    default: break;
  }
}

void decal_blood(float x, float y, float size) {
  /* (a room keeps the last few) */
  int n = 0, oldest = -1;
  for (int i = 0; i < MAX_ENTS; i++)
    if (ents[i].kind == EK_DECAL) {
      if (oldest < 0) oldest = i;
      n++;
    }
  if (n >= 14 && oldest >= 0) ents[oldest].kind = EK_NONE;
  Ent *e = ent_new(EK_DECAL, x, y);
  if (e) e->a = size;
}

Ent *creep_spawn(float x, float y, float r, int life, bool enemy, int color) {
  Ent *e = ent_new(EK_CREEP, x, y);
  if (!e) return NULL;
  e->r = r, e->t = (int16_t)life, e->type = (uint8_t)color, e->state = enemy;
  return e;
}

void creep_update(Ent *e) {
  if (--e->t <= 0) {
    e->kind = EK_NONE;
    return;
  }
  if (e->state) {   /* a monster's: it hurts Isaac's feet */
    float dx = pl.x - e->x, dy = (pl.y - e->y) / 0.6f;
    if (!pl.flight && dx * dx + dy * dy < e->r * e->r) player_hurt(1, -12);
  } else if ((e->t & 3) == 0) {   /* Isaac's (Lemon Mishap): it hurts theirs */
    for (int i = 0; i < MAX_ENTS; i++) {
      Ent *m = &ents[i];
      if (m->kind != EK_MON || (m->flags & EF_FLYING)) continue;
      float dx = m->x - e->x, dy = (m->y - e->y) / 0.6f;
      if (dx * dx + dy * dy < e->r * e->r) ent_damage(m, 3, 0, 0, 0);
    }
  }
}

/* the way down: to the next floor */
void trapdoor_update(Ent *e) {
  if (e->t < 30) {
    e->t++;
    return;
  }
  float dx = pl.x - e->x, dy = pl.y - e->y;
  if (dx * dx + dy * dy < 8 * 8) {
    if (e->type == 1) fl.alt = 1;   /* the beam: the Cathedral */
    else if (fl.stage == 8) fl.alt = 0;
    game_next_floor();
  }
}
