/* Altered for NumPlay's Hill Climb Racing (NumWorks calculator): b2Log prints nothing; on a computer (tests) the
 * memory in use is counted */
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

#include <Box2D/Common/b2Settings.h>
#include <cstdlib>
#include <cstdarg>

b2Version b2_version = {2, 2, 1};

// Memory allocators. Modify these to use your own allocator.
#ifdef HOST
/* (tests: Box2D's memory in use and the most it has used, a word before each block keeping its size) */
extern "C" { long b2_heap_now, b2_heap_peak; }
void* b2Alloc(int32 size)
{
	long *p = (long *)malloc(size + sizeof(long) * 2);
	if (!p) return NULL;
	p[0] = size;
	b2_heap_now += size;
	if (b2_heap_now > b2_heap_peak) b2_heap_peak = b2_heap_now;
	return p + 2;
}

void b2Free(void* mem)
{
	if (!mem) return;
	long *p = (long *)mem - 2;
	b2_heap_now -= p[0];
	free(p);
}
#else
void* b2Alloc(int32 size)
{
	return malloc(size);
}

void b2Free(void* mem)
{
	free(mem);
}
#endif

// You can modify this to use your logging facility.
void b2Log(const char* string, ...)
{
	(void)string;
}