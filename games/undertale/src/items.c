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
  [ITEM_TOUGH_GLOVE] = {"Tough Glove", "TuffGlove", IT_WEAPON, 0, 5, 0, "A worn pink leather glove.\n* For five-fingered folk.", "", 50},
  [ITEM_MANLY_BANDANNA] = {"Manly Bandanna", "Mandanna", IT_ARMOR, 0, 0, 7, "It has seen some wear.\n* It has abs drawn on it.", "", 50},
  [ITEM_SNOWMAN_PIECE] = {"Snowman Piece", "SnowPiece", IT_FOOD, 45, 0, 0, "Please take this to the\n  ends of the earth.",
                          "* You ate the Snowman Piece.", 40},
  [ITEM_NICE_CREAM] = {"Nice Cream", "NiceCream", IT_FOOD, 15, 0, 0, "Instead of a joke, the\n  wrapper says something nice.",
                       "* You unwrapped the Nice Cream.", 2},
  [ITEM_BISICLE] = {"Bisicle", "Bisicle", IT_FOOD, 11, 0, 0, "It's a two-pronged popsicle,\n  so you can eat it twice.",
                    "* You eat one half of the\n  Bisicle.", 5},
  [ITEM_UNISICLE] = {"Unisicle", "Unisicle", IT_FOOD, 11, 0, 0, "It's a SINGLE-pronged popsicle.\n* Wait, that's just normal...",
                     "* You ate the Unisicle.", 2},
  [ITEM_CINNAMON_BUNNY] = {"Cinnamon Bunny", "CinnaBun", IT_FOOD, 22, 0, 0, "A cinnamon roll in the\n  shape of a bunny.",
                           "* You eat the Cinnamon Bunny.", 8},
  [ITEM_OLD_TUTU] = {"Old Tutu", "Old Tutu", IT_ARMOR, 0, 0, 10, "A dusty frilly skirt.\n* Still holds up well.", "", 80},
  [ITEM_BALLET_SHOES] = {"Ballet Shoes", "BallShoes", IT_WEAPON, 0, 7, 0, "Worn dancing shoes.\n* Too big for you.", "", 80},
  [ITEM_QUICHE] = {"Abandoned Quiche", "Quiche", IT_FOOD, 34, 0, 0, "A quiche someone left\n  under a bench.",
                   "* You eat the quiche.", 76},
  [ITEM_PUNCH_CARD] = {"Punch Card", "PunchCard", IT_SPECIAL, 0, 0, 0, "A card for a game nobody\n  plays anymore.",
                       "* You look at the punch card.\n* It makes you want to punch\n  something.", 15},
  [ITEM_ARTIFACT] = {"Legendary Artifact", "Artifact", IT_SPECIAL, 0, 0, 0, "A mysterious orb.\n* It's a little heavy.",
                     "* You hold the orb up.\n* Nothing happens.", 0},
  [ITEM_ASTRONAUT_FOOD] = {"Astronaut Food", "AstroFood", IT_FOOD, 21, 0, 0, "Food for a trip to space,\n  found in a cooler.",
                           "* You eat the Astronaut Food.", 25},
  [ITEM_CRAB_APPLE] = {"Crab Apple", "CrabApple", IT_FOOD, 18, 0, 0, "A fruit with little claws.",
                       "* You eat the Crab Apple.", 5},
  [ITEM_SEA_TEA] = {"Sea Tea", "SeaTea", IT_FOOD, 10, 0, 0, "Tea made from sea grass.",
                    "* You drink the Sea Tea.\n* Your feet feel lighter.", 5},
  [ITEM_CLOUDY_GLASSES] = {"Cloudy Glasses", "Glasses", IT_ARMOR, 0, 0, 5, "Glasses you can barely\n  see through.", "", 30},
  [ITEM_TORN_NOTEBOOK] = {"Torn Notebook", "TornNotbo", IT_WEAPON, 0, 2, 0, "An old notebook, torn up.\n* It doesn't hit very hard.", "", 50},
  [ITEM_TEMMIE_FLAKES] = {"Temmie Flakes", "TemFlakes", IT_FOOD, 2, 0, 0, "Bits of paper, sold as\n  food.",
                          "* You eat the Temmie Flakes.", 2},
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
    case IT_SPECIAL: snprintf(out, (size_t)cap, "* \"%s\"\n* %s", it->name, it->info); break;
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
    case IT_SPECIAL:   /* looked at, kept */
      snprintf(out, (size_t)cap, "%s", it->use);
      return;
    default: {
      int n = snprintf(out, (size_t)cap, "%s", it->use);
      if (id == ITEM_NICE_CREAM) {   /* its wrapper's kind words */
        static const char *const nice[] = {"You're just great!", "You look nice today!", "Are those claws natural?",
                                           "You're super spiffy!", "Have a wonderful day!", "Is this as sweet as you?"};
        n += snprintf(out + n, (size_t)(cap - n), "\n* \"%s\"", nice[rnd_n(6)]);
      }
      /* half a Bisicle is a Unisicle */
      if (id == ITEM_BISICLE) sv.items[slot] = ITEM_UNISICLE;
      else item_remove_at(slot);
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
