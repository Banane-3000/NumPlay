/* Shops, as the game shows them: the keeper's picture over a text box and Buy, Sell, Talk, Exit; and the
 * Dimensional Box (an inventory you leave items in). Both are their own mode (MODE_SHOP). */
#include <stdio.h>
#include <string.h>
#include "event.h"
#include "game.h"
#include "gfx.h"
#include "items.h"
#include "res.h"
#include "shop.h"
#include "text.h"

typedef struct {
  uint8_t item;
  uint8_t price;
} Ware;
typedef struct {
  uint16_t pic;
  const char *hello, *again, *buying, *bought, *poor, *full, *nobuy, *sell;
  Ware wares[4];
  int nwares;
  const char *topics[4];
  const char *answers[4];
  int ntopics;
  bool buys;   /* Sell: it buys what you carry (the Tem Shop) */
} Shop;

static const Shop shops[SHOP_COUNT] = {
  [SHOP_SNOWDIN] = {
    .pic = SPR_SHOP_QC, .hello = "* Hello, traveller.\n* How can I help you?", .again = "* Take your time.",
    .buying = "What would\nyou like\nto buy?", .bought = "Thanks for\nyour purchase.", .poor = "That's not\nenough money.",
    .full = "You're\ncarrying\ntoo much.", .nobuy = "Just\nlooking?",
    .sell = "* Huh? Sell somethin'?\n* Does this look like a pawn shop?",
    .wares = {{ITEM_TOUGH_GLOVE, 50}, {ITEM_MANLY_BANDANNA, 50}, {ITEM_BISICLE, 15}, {ITEM_CINNAMON_BUNNY, 25}}, .nwares = 4,
    .topics = {"Say hello", "What to do here", "Town history", "Your life"},
    .answers = {"* Hello! Welcome to Snowdin!\n* I can't remember the last time I saw a new face around here.",
                "* If you're tired, the inn's right next door. My sister runs it.|* And there's Grillby's, if you're hungry.",
                "* Long ago, monsters lived in the RUINS back there.|* When we came here, the cold made it easy to get used to the snow.",
                "* Life is the same as usual.\n* A little cold, though..."},
    .ntopics = 4},
  [SHOP_GERSON] = {
    .pic = SPR_SHOP_GERSON, .hello = "* Hello there, little one!\n* Pay no mind to the junk.", .again = "* Take a look around!",
    .buying = "Take your\ntime.", .bought = "Wa ha ha!\nThank you!", .poor = "Not enough\nmoney, eh?",
    .full = "Your pockets\nare full,\nkid.", .nobuy = "Hmm?",
    .sell = "* Sell? I'm not that kind of shop, kid.\n* Try the Temmies, if you can find them.",
    .wares = {{ITEM_CRAB_APPLE, 25}, {ITEM_SEA_TEA, 18}, {ITEM_CLOUDY_GLASSES, 35}, {ITEM_TORN_NOTEBOOK, 55}}, .nwares = 4,
    .topics = {"About you", "About Undyne", "About the king", "About the junk"},
    .answers = {"* Me? Just an old turtle who's seen a lot.\n* They used to call me a hero, way back when.",
                "* Undyne? That girl's the head of the Royal Guard.\n* She'd fight anyone to protect us. Wa ha ha!",
                "* The king's a big softie.\n* Lost his family a long while back. Still keeps smiling.",
                "* All of this washed up in the dump.\n* One monster's trash is another's treasure!"},
    .ntopics = 4},
  [SHOP_TEM] = {
    .pic = SPR_SHOP_TEM, .hello = "* hOI!\n* welcom to... da TEM SHOP!!!", .again = "* hOI!", .buying = "tem has\nstuff!!",
    .bought = "thanks!!!", .poor = "no muns...", .full = "u hav too\nmany stuffs", .nobuy = "oh...",
    .sell = "", .buys = true,
    .wares = {{ITEM_TEMMIE_FLAKES, 1}}, .nwares = 1,
    .topics = {"Say hello", "About Temmies", "Tem history", "About shop"},
    .answers = {"* hOI!!! i'm temmie!!", "* temmies r the best!!!\n* we love humans... but some temmies r allergic...",
                "* temmie village is very old...\n* tem forget the rest.", "* tem wants to go to colleg someday!!"},
    .ntopics = 4},
};

static int shop_id, st, sel, sub;
static Writer w;
static char buf[300];
static const char *right_text;
static bool is_box;
enum { S_MENU, S_BUY, S_CONFIRM, S_TALK, S_SAY, S_SELL };

static void left_say(const char *s) {
  snprintf(buf, sizeof buf, "%s", s);
  text_wrap_width(190);
  writer_start(&w, buf, 12, 133, FONT_MAIN);
  w.line_h = 16;
}

void shop_open(int id) {
  shop_id = id, is_box = false;
  st = S_MENU, sel = 0, sub = 0, right_text = NULL;
  left_say(shops[id].hello);
  g_mode = MODE_SHOP;
}

/* the Bisicle costs more after each boss fought (the game's 15G and up) */
static int price_of(const Ware *wa) {
  if (wa->item == ITEM_BISICLE) return min_i(70, wa->price + 5 * flag(F_SNOWDIN_BOSSES));
  return wa->price;
}

/* ---------------------------------------------------------------- the Dimensional Box */
#define BOX_SLOTS 10
void box_open(void) {
  is_box = true, sel = 0, sub = 0;
  g_mode = MODE_SHOP;
}
static int box_item(int i) { return flag(F_BOX0 + i); }

static void box_update(uint32_t pressed) {
  int n = sub ? BOX_SLOTS : NUM_ITEMS;
  if (pressed & K_UP) sel = (sel + n - 1) % n;
  if (pressed & K_DOWN) sel = (sel + 1) % n;
  if (pressed & (K_LEFT | K_RIGHT)) sub = !sub, sel = min_i(sel, (sub ? BOX_SLOTS : NUM_ITEMS) - 1);
  if (pressed & K_Z) {
    if (!sub && sv.items[sel]) {
      for (int i = 0; i < BOX_SLOTS; i++)
        if (!box_item(i)) {
          set_flag(F_BOX0 + i, sv.items[sel]);
          item_remove_at(sel);
          break;
        }
    } else if (sub && box_item(sel) && item_add(box_item(sel))) {
      /* the box keeps its order: the ones after move up */
      for (int i = sel; i < BOX_SLOTS - 1; i++) set_flag(F_BOX0 + i, box_item(i + 1));
      set_flag(F_BOX0 + BOX_SLOTS - 1, 0);
    }
  }
  if (pressed & K_X) g_mode = MODE_WORLD;
}

static void panel(int x, int y, int w_, int h) {
  gfx_rect(x, y, w_, h, C_BLACK, 255);
  gfx_frame_rect(x, y, w_, h, 3, C_WHITE);
}

static void box_draw(void) {
  gfx_rect(0, 0, SCREEN_W, SCREEN_H, C_BLACK, 255);
  panel(8, 8, 304, 224);
  gfx_text(FONT_MENU, 50, 18, "INVENTORY", C_WHITE, 255);
  gfx_text(FONT_MENU, 210, 18, "BOX", C_WHITE, 255);
  gfx_line(160, 20, 160, 210, C_WHITE);
  for (int i = 0; i < NUM_ITEMS; i++) {
    const char *nm = sv.items[i] ? item_info(sv.items[i])->name : "";
    if (sv.items[i]) gfx_text(FONT_MENU, 36, 40 + i * 20, nm, C_WHITE, 255);
    else gfx_rect(36, 48 + i * 20, 100, 1, rgb565(0xC00000), 255);
  }
  for (int i = 0; i < BOX_SLOTS; i++) {
    if (box_item(i)) gfx_text(FONT_MENU, 190, 36 + i * 17, item_info(box_item(i))->name, C_WHITE, 255);
    else gfx_rect(190, 44 + i * 17, 100, 1, rgb565(0xC00000), 255);
  }
  gfx_sprite(SPR_HEART, 0, sub ? 180 : 26, sub ? 42 + sel * 17 : 46 + sel * 20, 0, 255);
  gfx_text(FONT_MENU, 120, 214, "Press [X] to Finish", C_WHITE, 255);
}

/* ---------------------------------------------------------------- the shop */
void shop_update(uint32_t keys, uint32_t pressed) {
  (void)keys;
  if (is_box) {
    box_update(pressed);
    return;
  }
  const Shop *s = &shops[shop_id];
  bool typing = writer_update(&w, st == S_SAY ? pressed : 0);
  switch (st) {
    case S_MENU:
      if (pressed & K_UP) sel = (sel + 3) % 4;
      if (pressed & K_DOWN) sel = (sel + 1) % 4;
      if (pressed & K_Z) {
        if (sel == 0) st = S_BUY, sub = 0, right_text = s->buying;
        else if (sel == 1 && s->buys) st = S_SELL, sub = 0, right_text = "what u\nsell??";
        else if (sel == 1) st = S_SAY, left_say(s->sell);
        else if (sel == 2) st = S_TALK, sub = 0;
        else g_mode = MODE_WORLD;
      }
      break;
    case S_BUY:
      if (pressed & K_UP) sub = (sub + s->nwares) % (s->nwares + 1);
      if (pressed & K_DOWN) sub = (sub + 1) % (s->nwares + 1);
      if (pressed & K_X || ((pressed & K_Z) && sub == s->nwares)) {
        st = S_MENU, left_say(s->again);
        break;
      }
      if (pressed & K_Z) st = S_CONFIRM, sel = 0;
      break;
    case S_CONFIRM:
      if (pressed & (K_UP | K_DOWN)) sel = !sel;
      if (pressed & K_X) st = S_BUY, right_text = s->nobuy;
      else if (pressed & K_Z) {
        const Ware *wa = &s->wares[sub];
        if (sel == 1) right_text = s->nobuy;
        else if (sv.gold < price_of(wa)) right_text = s->poor;
        else if (!item_add(wa->item)) right_text = s->full;
        else sv.gold = (uint16_t)(sv.gold - price_of(wa)), right_text = s->bought;
        st = S_BUY;
      }
      break;
    case S_TALK:
      if (pressed & K_UP) sub = (sub + s->ntopics) % (s->ntopics + 1);
      if (pressed & K_DOWN) sub = (sub + 1) % (s->ntopics + 1);
      if (pressed & K_X || ((pressed & K_Z) && sub == s->ntopics)) {
        st = S_MENU, left_say(s->again);
        break;
      }
      if (pressed & K_Z) st = S_SAY, left_say(s->answers[sub]);
      break;
    case S_SAY:
      if (!typing) st = S_MENU, left_say(s->again);
      break;
    case S_SELL: {
      /* what's carried, and Exit: Z sells it for what it's worth */
      if (pressed & K_UP) sub = (sub + NUM_ITEMS) % (NUM_ITEMS + 1);
      if (pressed & K_DOWN) sub = (sub + 1) % (NUM_ITEMS + 1);
      if (pressed & K_X || ((pressed & K_Z) && sub == NUM_ITEMS)) {
        st = S_MENU, left_say(s->again);
        break;
      }
      if ((pressed & K_Z) && sv.items[sub]) {
        sv.gold = (uint16_t)min_i(9999, sv.gold + item_info(sv.items[sub])->sell);
        item_remove_at(sub);
        right_text = "thanks!!";
      }
      break;
    }
  }
}

static void wares_list(const Shop *s, bool topics) {
  int n = topics ? s->ntopics : s->nwares;
  for (int i = 0; i <= n; i++) {
    char line[48];
    if (i == n) snprintf(line, sizeof line, "Exit");
    else if (topics) snprintf(line, sizeof line, "%s", s->topics[i]);
    else snprintf(line, sizeof line, "%dG - %s", price_of(&s->wares[i]), item_info(s->wares[i].item)->name);
    gfx_text(FONT_MAIN, 30, 132 + i * 18, line, C_WHITE, 255);
  }
  if (!(st == S_CONFIRM)) gfx_sprite(SPR_HEART, 0, 20, 140 + sub * 18, 0, 255);
}

static void sell_list(void) {
  for (int i = 0; i <= NUM_ITEMS; i++) {
    char line[48];
    if (i == NUM_ITEMS) snprintf(line, sizeof line, "Exit");
    else if (sv.items[i]) snprintf(line, sizeof line, "%dG - %s", item_info(sv.items[i])->sell, item_info(sv.items[i])->short_name);
    else line[0] = 0;
    gfx_text(FONT_SMALL, 30, 127 + i * 12, line, C_WHITE, 255);
  }
  gfx_sprite(SPR_HEART, 0, 20, 130 + sub * 12, 0, 255);
}

void shop_draw(void) {
  if (is_box) {
    box_draw();
    return;
  }
  const Shop *s = &shops[shop_id];
  gfx_rect(0, 0, SCREEN_W, SCREEN_H, C_BLACK, 255);
  gfx_sprite(s->pic, 0, 0, 0, 0, 255);
  panel(2, 121, 210, 117);
  panel(212, 121, 106, 117);
  char g[24];
  snprintf(g, sizeof g, "%dG", sv.gold);
  gfx_text(FONT_MAIN, 226, 214, g, C_WHITE, 255);
  snprintf(g, sizeof g, "%d/8", item_count());
  gfx_text(FONT_MAIN, 276, 214, g, C_WHITE, 255);
  if (st == S_MENU || st == S_SAY) {
    writer_draw(&w, C_WHITE);
    static const char *const opts[] = {"Buy", "Sell", "Talk", "Exit"};
    for (int i = 0; i < 4; i++) gfx_text(FONT_MAIN, 238, 132 + i * 20, opts[i], C_WHITE, 255);
    if (st == S_MENU) gfx_sprite(SPR_HEART, 0, 228, 140 + sel * 20, 0, 255);
    return;
  }
  if (st == S_SELL) {
    sell_list();
    if (right_text) gfx_text(FONT_SMALL, 222, 130, right_text, C_WHITE, 255);
    return;
  }
  wares_list(s, st == S_TALK);
  if (st == S_CONFIRM) {
    char q[40];
    snprintf(q, sizeof q, "Buy it for\n%dG?", price_of(&s->wares[sub]));
    gfx_text(FONT_MAIN, 222, 132, q, C_WHITE, 255);
    gfx_text(FONT_MAIN, 238, 172, "Yes", C_WHITE, 255);
    gfx_text(FONT_MAIN, 238, 190, "No", C_WHITE, 255);
    gfx_sprite(SPR_HEART, 0, 228, 180 + sel * 18, 0, 255);
  } else if (st == S_BUY) {
    if (sub < s->nwares) {
      /* what the item is: its kind and what it does */
      const ItemInfo *it = item_info(s->wares[sub].item);
      char info[64];
      if (it->type == IT_WEAPON) snprintf(info, sizeof info, "Weapon:\nAT %d", it->at);
      else if (it->type == IT_ARMOR) snprintf(info, sizeof info, "Armor:\nDF %d", it->df);
      else snprintf(info, sizeof info, "Heals\n%d HP", it->heal);
      gfx_text(FONT_MAIN, 222, 172, info, C_WHITE, 255);
    }
    if (right_text) gfx_text(FONT_SMALL, 222, 130, right_text, C_WHITE, 255);
  }
}

/* the Dimensional Box as an event: it opens, and the event waits for it to close */
int box_menu(Ev *c) {
  EV_BEGIN;
  box_open();
  WAIT_UNTIL(g_mode == MODE_WORLD);
  EV_END;
}
