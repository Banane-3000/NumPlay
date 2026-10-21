/* Semaphores between the RP2040's two cores. The calculator has one, so a semaphore is only a
 * count, and nothing ever waits on one that is not free. */
#ifndef NW_PICO_SEM_H
#define NW_PICO_SEM_H
#include "pico.h"

typedef struct {
  int16_t permits, max_permits;
} semaphore_t;

static inline void sem_init(semaphore_t *s, int16_t initial, int16_t max) {
  s->permits = initial;
  s->max_permits = max;
}
static inline bool sem_available(semaphore_t *s) { return s->permits > 0; }
static inline bool sem_release(semaphore_t *s) {
  if (s->permits >= s->max_permits) return false;
  s->permits++;
  return true;
}
static inline void sem_acquire_blocking(semaphore_t *s) {
  if (s->permits > 0) s->permits--;
}
static inline bool sem_try_acquire(semaphore_t *s) {
  if (s->permits <= 0) return false;
  s->permits--;
  return true;
}
static inline bool sem_acquire_timeout_ms(semaphore_t *s, uint32_t ms) {
  (void)ms;
  return sem_try_acquire(s);
}

#endif
