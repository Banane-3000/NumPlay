/* (being written) */
#include "s1.h"
#include "game.h"

void DebugMode(Obj *o) { (void)o; }
void GM_Continue(void) { V.gamemode = id_Sega; }
void GM_Ending(void) { V.gamemode = id_Sega; }
void GM_Credits(void) { V.gamemode = id_Sega; }
