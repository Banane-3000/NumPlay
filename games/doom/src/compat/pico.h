/* The few words of the Raspberry Pi Pico SDK the engine still says, on the calculator. */
#ifndef NW_PICO_H
#define NW_PICO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#ifndef __aligned
#define __aligned(n) __attribute__((aligned(n)))
#endif
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#define __noinline __attribute__((noinline))
#define __not_in_flash_func(f) f
#define __no_inline_not_in_flash_func(f) __attribute__((noinline)) f
#define __scratch_x(s)
#define __scratch_y(s)
#define __time_critical_func(f) f
#define __force_inline inline __attribute__((always_inline))
#define __compiler_memory_barrier() __asm__ volatile("" ::: "memory")
#define __breakpoint() I_Error("breakpoint")
#define tight_loop_contents() ((void)0)
#ifndef count_of
#define count_of(a) (sizeof(a) / sizeof((a)[0]))
#endif
#define hard_assert(x) assert(x)
#define invalid_params_if(x, test) ((void)0)
#define CU_REGISTER_DEBUG_PINS(...)
#define CU_SELECT_DEBUG_PINS(...)
#define DEBUG_PINS_SET(p, v) ((void)0)
#define DEBUG_PINS_CLR(p, v) ((void)0)
#define DEBUG_PINS_XOR(p, v) ((void)0)
#define panic I_Error
static inline unsigned get_core_num(void) { return 0; }
#define __mul_instruction(a, b) ((a) * (b))
typedef int spin_lock_t;
#define spin_lock_instance(n) ((spin_lock_t *)0)
#define spin_lock_blocking(l) 0u
#define spin_unlock(l, s) ((void)(s))

#endif
