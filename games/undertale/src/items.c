/* Items: what they are and what using one does (items.h). Their texts are the game's. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "items.h"

static const ItemInfo items[ITEM_COUNT] = {
  [ITEM_NONE] = {"", "", IT_SPECIAL, 0, 0, 0, "", "", 0},
  [ITEM_MONSTER_CANDY] = {"Monster Candy", "MnstrCndy", IT_FOOD, 10, 0, 0, "Has a distinct,\n  non-licorice flavor.",
                          "* You ate the Monster Candy.\n* Very un-licorice-like.", 25},
  [ITEM_STICK] = {"Stick", "Stick", IT_WEAPON, 0, 0, 0, "Its bark is worse than\n  its bite.",
                  "* You threw the stick away.\n* Then picked it back up.", 150},
  [ITEM_BANDAGE] = {"Bandage", "Bandage", IT_ARMOR, 10, 0, 0, "It has already been used\n  several times.",
                    "* You re-applied the bandage.\n* Still kind of gooey.", 150},
  [ITEM_SPIDER_DONUT] = {"Spider Donut", "SpidrDont", IT_FOOD, 12, 0, 0, "A donut made with Spider\n  Cider in the batter.",
                         "* You ate the Spider Donut.", 30},
  [ITEM_SPIDER_CIDER] = {"Spider Cider", "SpidrCidr", IT_FOOD, 24, 0, 0, "Made with whole spiders,\n  not just the juice.",
                         "* You drank the Spider Cider.", 60},
  [ITEM_BUTTERSCOTCH_PIE] = {"Butterscotch Pie", "ButtsPie", IT_FOOD, 999, 0, 0, "Butterscotch-cinnamon\n  pie, one slice.",
                             "* You ate the Butterscotch\n  Pie.", 180},
  [ITEM_SNAIL_PIE] = {"Snail Pie", "SnailPie", IT_FOOD, 998, 0, 0, "An acquired taste.",
                      "* You ate the Snail Pie.\n* Your HP was maxed. Almost.", 350},
  [ITEM_FADED_RIBBON] = {"Faded Ribbon", "Ribbon", IT_ARMOR, 0, 0, 3, "If you're cuter, monsters\n  won't hit you as hard.", "", 0},
  [ITEM_TOY_KNIFE] = {"Toy Knife", "Toy Knife", IT_WEAPON, 0, 3, 0, "Made of plastic.\n* A rarity nowadays.", "", 100},
};

const ItemInfo *item_info(int id) { return &items[id >= 0 && id < ITEM_COUNT ? id : 0]; }

void item_info_text(int id, char *out, int cap) {
  const ItemInfo *it = item_info(id);
  switch (it->type) {
    case IT_WEAPON: snprintf(out, (size_t)cap, "* \"%s\" - Weapon AT %d\n* %s", it->name, it->at, it->info); break;
    case IT_ARMOR:
      if (id == ITEM_BANDAGE) snprintf(out, (size_t)cap, "* \"%s\" - Heals %d HP\n* %s", it->name, it->heal, it->info);
      else snprintf(out, (size_t)cap, "* \"%s\" - Armor DF %d\n* %s", it->name, it->df, it->info);
      break;
    default:
      if (it->heal >= 998) snprintf(out, (size_t)cap, "* \"%s\" - Heals ALL HP\n* %s", it->name, it->info);
      else snprintf(out, (size_t)cap, "* \"%s\" - Heals %d HP\n* %s", it->name, it->heal, it->info);
      break;
  }
}

/* heals and says so: "You recovered N HP!" or "Your HP was maxed out." */
static int heal(int n, char *out, int cap) {
  int before = sv.hp;
  sv.hp = (int16_t)min_i(sv.maxhp, sv.hp + n);
  if (sv.hp >= sv.maxhp) return snprintf(out, (size_t)cap, "\n* Your HP was maxed out.");
  return snprintf(out, (size_t)cap, "\n* You recovered %d HP!", sv.hp - before);
}

void item_use(int slot, char *out, int cap, int in_battle) {
  int id = sv.items[slot];
  const ItemInfo *it = item_info(id);
  out[0] = 0;
  switch (it->type) {
    case IT_WEAPON: {
      if (id == ITEM_STICK) {
        snprintf(out, (size_t)cap, "%s", it->use);
        return;
      }
      int old = sv.weapon;
      sv.weapon = (uint8_t)id;
      sv.items[slot] = (uint8_t)(old == ITEM_STICK ? 0 : old);
      if (!sv.items[slot]) item_remove_at(slot);
      snprintf(out, (size_t)cap, "* You equipped the %s.", it->name);
      return;
    }
    case IT_ARMOR:
      if (id == ITEM_BANDAGE && sv.armor != ITEM_BANDAGE) {
        int n = snprintf(out, (size_t)cap, "%s", it->use);
        item_remove_at(slot);
        heal(it->heal, out + n, cap - n);
        return;
      } else {
        int old = sv.armor;
        sv.armor = (uint8_t)id;
        sv.items[slot] = (uint8_t)old;
        if (!old) item_remove_at(slot);
        snprintf(out, (size_t)cap, "* You equipped the %s.", it->name);
        return;
      }
    default: {
      int n = snprintf(out, (size_t)cap, "%s", it->use);
      item_remove_at(slot);
      if (it->heal >= 999) {
        sv.hp = sv.maxhp;
        snprintf(out + n, (size_t)(cap - n), "\n* Your HP was maxed out.");
      } else if (it->heal == 998) {
        if (sv.hp < sv.maxhp - 1) sv.hp = (int16_t)(sv.maxhp - 1);
      } else {
        heal(it->heal, out + n, cap - n);
      }
      (void)in_battle;
      return;
    }
  }
}
