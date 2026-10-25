/* Altered for NumPlay's Angry Birds (NumWorks calculator): b2Alloc takes from the arena (b2BlockAllocator.h), a
 * word before each block keeping its size; b2Log prints nothing */
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
#include <Box2D/Common/b2BlockAllocator.h>
#include <cstddef>

b2Version b2_version = {2, 2, 1};

int32 b2_treeCapacity = 16, b2_moveCapacity = 16, b2_pairCapacity = 16;

void* b2Alloc(int32 size)
{
	int32* p = (int32*)b2ArenaAlloc(size + 8);
	if (p == NULL)
	{
		return NULL;
	}
	p[0] = size + 8;
	return p + 2;
}

void b2Free(void* mem)
{
	if (mem == NULL)
	{
		return;
	}
	int32* p = (int32*)mem - 2;
	b2ArenaRelease(p, p[0]);
}

void b2Log(const char* string, ...)
{
	(void)string;
}
