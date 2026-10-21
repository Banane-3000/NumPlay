/* Doom on the calculator: sound and music. The calculator has no speaker, so the game plays in
 * silence: what asks for a sound is told it is done. */
#include <stddef.h>

#include "doomtype.h"
#include "i_sound.h"

int snd_samplerate = 44100;
int snd_cachesize = 0;
int snd_maxslicetime_ms = 28;
should_be_const constcharstar snd_musiccmd = "";
should_be_const constcharstar snd_dmxoption = "";
isb_int8_t snd_pitchshift = 0;

void I_InitSound(boolean use_sfx_prefix) {}
void I_ShutdownSound(void) {}
int I_GetSfxLumpNum(should_be_const sfxinfo_t *sfxinfo) { return 0; }
void I_UpdateSound(void) {}
void I_UpdateSoundParams(int channel, int vol, int sep) {}
int I_StartSound(should_be_const sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch) { return -1; }
void I_StopSound(int channel) {}
boolean I_SoundIsPlaying(int channel) { return false; }
void I_PrecacheSounds(should_be_const sfxinfo_t *sounds, int num_sounds) {}
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_SetMusicVolume(int volume) {}
void I_PauseSong(void) {}
void I_ResumeSong(void) {}
void *I_RegisterSong(should_be_const void *data, int len) { return NULL; }
void I_UnRegisterSong(void *handle) {}
void I_PlaySong(void *handle, boolean looping) {}
void I_StopSong(void) {}
boolean I_MusicIsPlaying(void) { return false; }
void I_BindSoundVariables(void) {}
void I_SetOPLDriverVer(opl_driver_ver_t ver) {}
