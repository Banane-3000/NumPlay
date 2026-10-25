/* Altered for NumPlay's Angry Birds (NumWorks calculator): all of Box2D's memory comes from one arena the game
 * gives it (b2ArenaReset): blocks from its bottom, kept for reuse by their exact size, and the step's stack
 * allocations from its top, or from a side buffer while it has room (b2ArenaSide: a step's buffer on the stack).
 * When they would meet, an allocation returns NULL, which Box2D's callers here handle (no contact for that pair this
 * step, or the island waits a step). */
/*
* Copyright (c) 2006-2009 Erin Catto http://www.box2d.org
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#ifndef B2_BLOCK_ALLOCATOR_H
#define B2_BLOCK_ALLOCATOR_H

#include <Box2D/Common/b2Settings.h>

/// The arena: base (8-byte aligned) and size; everything allocated before is forgotten.
void b2ArenaReset(void* base, int32 size);
/// The arena's start (8-byte aligned).
char* b2ArenaBase();
/// The arena made bigger (same base): only while nothing is on its top end.
void b2ArenaGrow(int32 size);
/// Bytes in use now (both ends), the most ever in use, the biggest push that would succeed (from the gap or the side
/// buffer) and the allocations that failed.
int32 b2ArenaUsed();
int32 b2ArenaPeak();
int32 b2ArenaFree();
int32 b2ArenaFailures();
/// Allocates from the bottom (NULL when full); freed blocks are reused by later blocks of the same size.
void* b2ArenaAlloc(int32 size);
void b2ArenaRelease(void* p, int32 size);
/// The top end, for the stack allocator (last in, first out).
void* b2ArenaPush(int32 size);
void b2ArenaPop(void* p, int32 size);
/// A side buffer (8-byte aligned) the top end's allocations come from first while they fit, or none (NULL); the
/// most of it ever used.
void b2ArenaSide(void* p, int32 size);
int32 b2ArenaSidePeak();
/// The room left in the side buffer.
int32 b2ArenaSideRoom();
/// An allocation that could not be made, counted with the failures (an island not solved for want of room).
void b2ArenaFailed();

#ifdef HOST
/// (tests) the arena's own bookkeeping, saved and put back with its bytes
int32 b2ArenaStateSize();
void b2ArenaStateSave(void* to);
void b2ArenaStateLoad(const void* from);
#endif

class b2BlockAllocator
{
public:
	b2BlockAllocator() {}
	~b2BlockAllocator() {}

	/// Allocate memory (NULL when the arena is full).
	void* Allocate(int32 size) { return b2ArenaAlloc(size); }

	/// Free memory.
	void Free(void* p, int32 size) { b2ArenaRelease(p, size); }

	void Clear() {}
};

#endif
