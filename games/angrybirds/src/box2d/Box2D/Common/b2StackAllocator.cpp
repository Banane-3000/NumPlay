/* Altered for NumPlay's Angry Birds (NumWorks calculator): see b2StackAllocator.h */
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

#include <Box2D/Common/b2StackAllocator.h>
#include <Box2D/Common/b2BlockAllocator.h>
#include <Box2D/Common/b2Math.h>

b2StackAllocator::b2StackAllocator()
{
	m_allocation = 0;
	m_maxAllocation = 0;
	m_entryCount = 0;
}

b2StackAllocator::~b2StackAllocator()
{
}

void* b2StackAllocator::Allocate(int32 size)
{
	if (m_entryCount >= b2_maxStackEntries)
	{
		return NULL;
	}
	char* data = (char*)b2ArenaPush(size);
	if (data == NULL)
	{
		return NULL;
	}
	b2StackEntry* entry = m_entries + m_entryCount;
	entry->size = size;
	entry->data = data;
	m_allocation += size;
	m_maxAllocation = b2Max(m_maxAllocation, m_allocation);
	++m_entryCount;
	return data;
}

void b2StackAllocator::Free(void* p)
{
	if (p == NULL || m_entryCount == 0)
	{
		return;
	}
	b2StackEntry* entry = m_entries + m_entryCount - 1;
	b2ArenaPop(entry->data, entry->size);
	m_allocation -= entry->size;
	--m_entryCount;
}

int32 b2StackAllocator::GetMaxAllocation() const
{
	return m_maxAllocation;
}
