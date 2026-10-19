/* The object pointer table: each object id's code (the original's Obj_Index). */
#include "s1.h"
#include "game.h"
#include "objs.h"

const ObjFn obj_index[0x8D + 1] = {
  [id_SonicPlayer] = SonicPlayer,
};
