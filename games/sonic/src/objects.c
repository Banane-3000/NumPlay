/* The object pointer table: each object id's code (the original's Obj_Index). */
#include "s1.h"
#include "game.h"
#include "objs.h"

const ObjFn obj_index[0x8D + 1] = {
  [id_SonicPlayer] = SonicPlayer,
  [id_Signpost] = Signpost,
  [id_Rings] = Rings,
  [id_RingLoss] = RingLoss,
  [id_Monitor] = Monitor,
  [id_PowerUp] = PowerUp,
  [id_ExplosionItem] = ExplosionItem,
  [id_Explosion] = Explosion,
  [id_Animals] = Animals,
  [id_Points] = Points,
  [id_HUD] = HUD,
  [id_TitleCard] = TitleCard,
  [id_GameOverCard] = GameOverCard,
  [id_GotThroughCard] = GotThroughCard,
  [id_Lamppost] = Lamppost,
  [id_ShieldItem] = ShieldItem,
  [id_Springs] = Springs,
  [id_Spikes] = Spikes,
  [id_GiantRing] = GiantRing,
  [id_RingFlash] = RingFlash,
  [id_HiddenBonus] = HiddenBonus,
  [id_Prison] = Prison,
  [id_Invisibarrier] = Invisibarrier,
  [id_Button] = Button,
  [id_WaterSound] = WaterSound,
  [id_EdgeWalls] = EdgeWalls,
};
