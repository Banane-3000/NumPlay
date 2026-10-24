/* Altered for NumPlay's Angry Birds (NumWorks calculator): the arena (see b2BlockAllocator.h) instead of chunks
 * from malloc */
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

#include <Box2D/Common/b2BlockAllocator.h>
#include <stdint.h>
#include <string.h>

// sizes are rounded to 8 bytes; blocks up to b2_arenaListMax bytes keep a free list each, bigger ones share one
// list searched for the same size
#define b2_arenaListMax 1024

struct b2FreeBlock
{
	b2FreeBlock* next;
	int32 size;
};

static char* s_base;
static int32 s_size, s_bottom, s_top, s_peak, s_failures, s_freeBytes, s_lead;
static b2FreeBlock* s_lists[b2_arenaListMax / 8 + 1];
static b2FreeBlock* s_large;
static char* s_side;
static int32 s_sideSize, s_sideTop, s_sidePeak;

static inline int32 b2RoundSize(int32 size)
{
	return size < 8 ? 8 : (size + 7) & ~7;
}

void b2ArenaReset(void* base, int32 size)
{
	uintptr_t b = ((uintptr_t)base + 7) & ~(uintptr_t)7;
	s_base = (char*)b;
	s_lead = (int32)(b - (uintptr_t)base);
	s_size = (size - s_lead) & ~7;
	s_bottom = 0;
	s_top = s_size;
	s_peak = 0;
	s_failures = 0;
	s_freeBytes = 0;
	s_sidePeak = 0;
	memset(s_lists, 0, sizeof(s_lists));
	s_large = NULL;
}

char* b2ArenaBase()
{
	return s_base;
}

void b2ArenaGrow(int32 size)
{
	if (s_top != s_size)
	{
		return;
	}
	size = (size - s_lead) & ~7;
	if (size > s_size)
	{
		s_size = size;
		s_top = size;
	}
}

int32 b2ArenaUsed()
{
	return s_bottom - s_freeBytes + (s_size - s_top);
}

int32 b2ArenaPeak()
{
	return s_peak;
}

int32 b2ArenaFree()
{
	int32 gap = s_top - s_bottom;
	return s_side && s_sideTop > gap ? s_sideTop : gap;
}

int32 b2ArenaFailures()
{
	return s_failures;
}

static inline void b2ArenaNote()
{
	int32 used = s_bottom + (s_size - s_top);
	if (used > s_peak)
	{
		s_peak = used;
	}
}

void* b2ArenaAlloc(int32 size)
{
	if (size <= 0)
	{
		return NULL;
	}
	size = b2RoundSize(size);
	if (size <= b2_arenaListMax)
	{
		b2FreeBlock* f = s_lists[size >> 3];
		if (f)
		{
			s_lists[size >> 3] = f->next;
			s_freeBytes -= size;
			return f;
		}
	}
	else
	{
		for (b2FreeBlock** f = &s_large; *f; f = &(*f)->next)
		{
			if ((*f)->size == size)
			{
				b2FreeBlock* found = *f;
				*f = found->next;
				s_freeBytes -= size;
				return found;
			}
		}
	}
	if (s_top - s_bottom < size)
	{
		++s_failures;
		return NULL;
	}
	void* p = s_base + s_bottom;
	s_bottom += size;
	b2ArenaNote();
	return p;
}

void b2ArenaRelease(void* p, int32 size)
{
	if (p == NULL || size <= 0)
	{
		return;
	}
	size = b2RoundSize(size);
	b2FreeBlock* f = (b2FreeBlock*)p;
	if ((char*)p + size == s_base + s_bottom)
	{
		s_bottom -= size;	// (the last block: given back to the gap)
		return;
	}
	f->size = size;
	s_freeBytes += size;
	if (size <= b2_arenaListMax)
	{
		f->next = s_lists[size >> 3];
		s_lists[size >> 3] = f;
	}
	else
	{
		f->next = s_large;
		s_large = f;
	}
}

void* b2ArenaPush(int32 size)
{
	size = b2RoundSize(size);
	if (s_side && s_sideTop >= size)
	{
		s_sideTop -= size;
		if (s_sideSize - s_sideTop > s_sidePeak)
		{
			s_sidePeak = s_sideSize - s_sideTop;
		}
		return s_side + s_sideTop;
	}
	if (s_top - s_bottom < size)
	{
		++s_failures;
		return NULL;
	}
	s_top -= size;
	b2ArenaNote();
	return s_base + s_top;
}

void b2ArenaPop(void* p, int32 size)
{
	if (p == NULL)
	{
		return;
	}
	size = b2RoundSize(size);
	if (s_side && (char*)p >= s_side && (char*)p < s_side + s_sideSize)
	{
		s_sideTop += size;
		return;
	}
	s_top += size;
}

void b2ArenaSide(void* p, int32 size)
{
	s_side = (char*)p;
	s_sideSize = p ? size & ~7 : 0;
	s_sideTop = s_sideSize;
}

int32 b2ArenaSidePeak()
{
	return s_sidePeak;
}

int32 b2ArenaSideRoom()
{
	return s_side ? s_sideTop : 0;
}

void b2ArenaFailed()
{
	++s_failures;
}

#ifdef HOST
struct b2ArenaState
{
	char* base;
	int32 size, bottom, top, peak, failures, freeBytes, lead;
	b2FreeBlock* lists[b2_arenaListMax / 8 + 1];
	b2FreeBlock* large;
};

int32 b2ArenaStateSize()
{
	return (int32)sizeof(b2ArenaState);
}

void b2ArenaStateSave(void* to)
{
	b2ArenaState* a = (b2ArenaState*)to;
	a->base = s_base, a->size = s_size, a->bottom = s_bottom, a->top = s_top, a->peak = s_peak;
	a->failures = s_failures, a->freeBytes = s_freeBytes, a->lead = s_lead, a->large = s_large;
	memcpy(a->lists, s_lists, sizeof(s_lists));
}

void b2ArenaStateLoad(const void* from)
{
	const b2ArenaState* a = (const b2ArenaState*)from;
	s_base = a->base, s_size = a->size, s_bottom = a->bottom, s_top = a->top, s_peak = a->peak;
	s_failures = a->failures, s_freeBytes = a->freeBytes, s_lead = a->lead, s_large = a->large;
	memcpy(s_lists, a->lists, sizeof(s_lists));
}
#endif
