#ifndef FNF_SAVE_H
#define FNF_SAVE_H
#include "game.h"
#define SAVE_BYTES (16+CHART_COUNT*8)
#define BACKUP_BYTES (SAVE_BYTES*2+64)
void save_encode(const Progress*,uint8_t*);
bool save_decode(Progress*,const uint8_t*,unsigned);
unsigned save_backup(const uint8_t*,char*);
bool save_restore(Progress*,const uint8_t*,unsigned);
#endif
