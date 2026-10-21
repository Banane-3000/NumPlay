/* Dr. Zomboss's Zombot: its animations (boss.c) and its drawing from its parts (boss_art.c). */
#ifndef PVZ_BOSS_ART_H
#define PVZ_BOSS_ART_H
#include <stdint.h>

enum {
  CL_ENTER, CL_IDLE, CL_SPAWN1, CL_SPAWN2, CL_SPAWN3, CL_SPAWN4, CL_SPAWN5, CL_BUNGEE_ENTER, CL_BUNGEE_LEAVE,
  CL_STOMP1, CL_STOMP2, CL_STOMP3, CL_STOMP4, CL_HEAD_ENTER, CL_HEAD_IDLE, CL_ATTACK1, CL_ATTACK2, CL_ATTACK3,
  CL_ATTACK4, CL_ATTACK5, CL_HEAD_LEAVE, CL_DEATH, CL_RV, N_CLIPS
};
typedef struct { uint16_t start, len; } BossClip;
extern const BossClip boss_clips[N_CLIPS];
#endif
