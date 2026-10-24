/* The songs on offer (app.h): story mode plays a week's songs as the game made them (the default variation, or the
 * Erect one for Erect and Nightmare); freeplay lists Boyfriend's songs or Pico's, as the game's two freeplay
 * characters do (data/players: the characters each one owns). */
#include "app.h"

const char *const DIFF_NAME[FNF_DIFFS] = {"EASY", "NORMAL", "HARD", "ERECT", "NIGHTMARE"};

int app_owner(unsigned chr) {
  if (chr >= FNF_CHARS) return 0;
  return fnf_char[chr].flags & FNF_CHAR_BF ? 1 : fnf_char[chr].flags & FNF_CHAR_PICO ? 2 : 0;
}

static int chart_in(const fnf_var_t *v, int diff) {
  for (int c = v->chart; c < v->chart + v->ncharts; c++)
    if (fnf_chart[c].diff == diff) return c;
  return -1;
}

int app_chart(unsigned song, int list, int diff) {
  if (song >= FNF_SONGS) return -1;
  const fnf_song_t *s = &fnf_song[song];
  for (int i = s->var; i < s->var + s->nvar; i++) {
    const fnf_var_t *v = &fnf_var[i];
    int owner = app_owner(v->chr[0]);
    /* Boyfriend's list has his songs and those of nobody's characters (LE SSERAFIM's), Pico's has his */
    if (list == LIST_PICO ? owner != 2 : owner == 2) continue;
    int c = chart_in(v, diff);
    if (c >= 0) return c;
  }
  return -1;
}

int app_story_chart(unsigned song, int diff) {
  if (song >= FNF_SONGS) return -1;
  const fnf_song_t *s = &fnf_song[song];
  for (int i = s->var; i < s->var + s->nvar; i++) {
    const fnf_var_t *v = &fnf_var[i];
    if (v->kind != FNF_VAR_DEFAULT && v->kind != FNF_VAR_ERECT) continue;
    int c = chart_in(v, diff);
    if (c >= 0) return c;
  }
  return -1;
}

/* Level.getDifficulties: those every song of the week has */
unsigned app_story_diffs(unsigned level) {
  unsigned bits = 0;
  for (int d = 0; d < FNF_DIFFS; d++) {
    bool all = true;
    for (int i = 0; i < fnf_level[level].n; i++)
      if (app_story_chart(fnf_level[level].song[i], d) < 0) all = false;
    if (all) bits |= 1u << d;
  }
  return bits ? bits : 1u << FNF_DIFF_NORMAL;
}

void app_build_list(app_t *a) {
  a->list_n = 0;
  for (unsigned s = 0; s < FNF_SONGS; s++)
    for (int d = 0; d < FNF_DIFFS; d++)
      if (app_chart(s, a->p->list, d) >= 0) {
        a->list[a->list_n++] = (uint8_t)s;
        break;
      }
}
