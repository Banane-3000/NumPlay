/* Saves: the adventure's progress (pvz.sav), the level being played when Home quits (pvzlevel.sav), and a
 * copy of both in a Python script (pvz_saves.py): installing apps keeps only scripts. */
#ifndef PVZ_SAVE_H
#define PVZ_SAVE_H
#include <stdbool.h>
#include <stdint.h>

enum {
  ITEM_SHOVEL = 1, ITEM_ALMANAC = 2, ITEM_SHOP = 4, ITEM_POOL_CLEANER = 8, ITEM_ROOF_CLEANER = 16, ITEM_FIRST_AID = 32,
  ITEM_TROPHY = 64,
};
typedef struct {
  uint8_t level;        /* the adventure's next level, 1 to 50 */
  uint8_t finished;     /* times the adventure was won */
  uint8_t slots;        /* seed slots: 6, more bought from Crazy Dave */
  uint8_t rakes;        /* the rake's uses left */
  uint16_t items;       /* ITEM_* */
  uint8_t declined;     /* times Crazy Dave's seed slot was turned down */
  uint8_t keys_seen;    /* 0: the keys sheet not seen yet */
  uint32_t money;
  uint32_t talks;       /* Crazy Dave's talks already heard (bit n) */
  uint8_t nlast, last[10];   /* the seeds chosen last */
  uint8_t pad[2];
} Progress;
extern Progress progress;

void save_restore_copy(void);   /* the saves back from the script, where installing erased them */
void progress_load(void);
bool progress_save(void);
bool board_save(void);          /* the level in progress */
bool board_load(void);          /* (and forgets it) */
bool board_saved(void);
void board_forget(void);
/* the plants found so far (the adventure's rewards) */
bool plant_unlocked(int kind);
int level_world(int level);
int level_stage(int level);
#endif
