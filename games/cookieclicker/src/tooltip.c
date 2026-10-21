/* The game's crate tooltips (Game.crateTooltip): an upgrade's or an achievement's icon, price, name
 * and tags, then its description and its quote. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

extern void upgrade_description(int u, char *out, int size);
extern void text_beautify(const char *src, char *dst, int size);
extern int upgrade_lumps(int u);

void tip_line(int x, int y, int w) {
  gfx_hgrad(x, y, w / 2, 1, BLACK, 0, 110);
  gfx_hgrad(x + w / 2, y, w - w / 2, 1, BLACK, 110, 0);
  gfx_hgrad(x, y + 1, w / 2, 1, WHITE, 0, 26);
  gfx_hgrad(x + w / 2, y + 1, w - w / 2, 1, WHITE, 26, 0);
}

int tip_tag(int x, int y, const char *s, uint16_t bg) {
  int w = gfx_text_width(F_SMALL, s) + 6;
  gfx_round_rect(x, y, w, 10, 2, bg, 220);
  gfx_text(F_SMALL, x + 3, y + 8, s, BLACK, 256);
  return w + 2;
}

static const ParaStyle PS_NAME = {F_NAME, F_NAME, F_NAME, WHITE, WHITE, WHITE, 12, 0};

int crate_tip(int type, int id, int context, int x, int y, int w, bool draw) {
  char buf[1400];
  int yy = y;
  const char *tags[4];
  uint16_t colors[4];
  int ntags = 0;
  bool mysterious = false;
  int icon;
  const char *name, *quote;
  int price_w = 0;
  if (type == 0) {
    const UpgradeRec *r = upgrade_rec(id);
    bool bought = bit_get(S.upgrades_bought, id);
    icon = r->icon, name = res_text(r->name), quote = res_text(r->quote);
    if (r->pool == POOL_PRESTIGE) tags[ntags] = "Heavenly", colors[ntags++] = HEX(0xefa438);
    else if (r->pool == POOL_TECH) tags[ntags] = "Tech", colors[ntags++] = HEX(0x36a4ff);
    else if (r->pool == POOL_COOKIE) tags[ntags] = "Cookie", colors[ntags++] = WHITE;
    else if (r->pool == POOL_TOGGLE) tags[ntags] = "Switch", colors[ntags++] = WHITE;
    else tags[ntags] = "Upgrade", colors[ntags++] = WHITE;
    if (bit_get(S.upgrades_vaulted, id)) tags[ntags] = "Vaulted", colors[ntags++] = HEX(0x4e7566);
    if (bought) tags[ntags] = r->pool == POOL_TECH ? "Researched" : r->kitten ? "Purrchased" : "Purchased",
      colors[ntags++] = WHITE;
    /* the price */
    int lumps = upgrade_lumps(id);
    double cost = r->pool == POOL_PRESTIGE ? r->price : upgrade_price(id);
    if (!(bought && context == CTX_STORE) && (lumps || cost > 0)) {
      char t[48];
      if (lumps) snprintf(t, sizeof t, "%d", lumps);
      else beautify(round(cost), 0, t);
      bool can = r->pool == POOL_PRESTIGE ? bought || S.heavenly_chips >= cost
                 : context == CTX_STORE   ? upgrade_can_buy(id)
                                          : true;
      price_w = gfx_text_width(F_SMALL, t) + 12;
      if (draw) {
        int im = lumps ? IM_LUMP_MONEY : r->pool == POOL_PRESTIGE ? IM_HEAVENLY_MONEY : IM_MONEY;
        gfx_image(im, x + w - price_w, yy, 256);
        gfx_text_outline(F_SMALL, x + w - price_w + 12, yy + 8, t,
                         can ? r->pool == POOL_PRESTIGE ? HEX(0x99dfff) : RGB(102, 255, 102) : RGB(255, 102, 102), 256);
      }
    }
    upgrade_description(id, buf, sizeof buf);
  } else {
    const AchievRec *r = achiev_rec(id);
    bool won = has_achiev(id);
    icon = r->icon, name = res_text(r->name), quote = res_text(r->quote);
    if (r->pool == APOOL_SHADOW) tags[ntags] = "Shadow Achievement", colors[ntags++] = HEX(0x9700cf);
    else tags[ntags] = "Achievement", colors[ntags++] = WHITE;
    tags[ntags] = won ? "Unlocked" : "Locked", colors[ntags++] = WHITE;
    mysterious = !won;
    text_beautify(res_text(r->desc), buf, sizeof buf);
  }
  if (mysterious) icon = ICON(0, 7), name = "???", quote = "", snprintf(buf, sizeof buf, "???");
  if (draw) gfx_icon(icon, x, yy, 256);
  int nw = w - 27 - price_w - 2;
  int nh = gfx_para(&PS_NAME, x + 27, yy, nw, name, false);
  if (draw) gfx_para(&PS_NAME, x + 27, yy, nw, name, true);
  int ty = yy + (nh > 12 ? nh : 12) + 1;
  if (draw) {
    int tx = x + 27;
    for (int i = 0; i < ntags; i++) tx += tip_tag(tx, ty, tags[i], colors[i]);
  }
  yy = ty + 12;
  if (yy < y + 26) yy = y + 26;
  if (draw) tip_line(x, yy, w);
  yy += 4;
  yy += gfx_para(&PS_TIP, x, yy, w, buf, draw) + 2;
  if (type == 0 && id == U_ELDER_PLEDGE && bit_get(S.upgrades_bought, id) && context == CTX_STORE) {
    char t[64], line[128];
    say_time(S.pledge_t, -1, t);
    snprintf(line, sizeof line, "Time remaining until pledge runs out:\n\x01%s\x02", t);
    yy += gfx_para(&PS_TIP, x, yy + 2, w, line, draw) + 4;
  }
  if (quote[0]) {
    char q[600];
    snprintf(q, sizeof q, CH_LQUOTE "%s" CH_RQUOTE, quote);
    ParaStyle qs = PS_QUOTE;
    qs.align = 1;
    yy += gfx_para(&qs, x + 6, yy + 3, w - 12, q, draw) + 5;
  }
  if (context == CTX_STORE && type == 0) {
    const UpgradeRec *r = upgrade_rec(id);
    const char *tip = r->pool == POOL_TOGGLE ? "Press EXE to toggle." : r->pool == POOL_TECH ? "Press EXE to research."
                      : r->kitten ? "Press EXE to purrchase." : "Press EXE to purchase.";
    if (draw) {
      tip_line(x, yy, w);
      gfx_text_center(F_SMALL, x + w / 2, yy + 11, tip, HEX(0x999999), 256);
    }
    yy += 14;
  }
  return yy - y;
}
