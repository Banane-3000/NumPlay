/* Altered for NumPlay's Angry Birds (NumWorks calculator): only the distance, revolute and weld joints (the ones
 * Angry Birds uses); Create returns NULL when the arena is full; no user data */
/*
* Copyright (c) 2006-2007 Erin Catto http://www.box2d.org
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

#include <Box2D/Dynamics/Joints/b2Joint.h>
#include <Box2D/Dynamics/Joints/b2DistanceJoint.h>
#include <Box2D/Dynamics/Joints/b2RevoluteJoint.h>
#include <Box2D/Dynamics/Joints/b2WeldJoint.h>
#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/b2World.h>
#include <Box2D/Common/b2BlockAllocator.h>

#include <new>

static int32 b2JointSize(b2JointType type)
{
	switch (type)
	{
	case e_distanceJoint:
		return sizeof(b2DistanceJoint);
	case e_revoluteJoint:
		return sizeof(b2RevoluteJoint);
	case e_weldJoint:
		return sizeof(b2WeldJoint);
	default:
		return 0;
	}
}

b2Joint* b2Joint::Create(const b2JointDef* def, b2BlockAllocator* allocator)
{
	int32 size = b2JointSize(def->type);
	void* mem = size ? allocator->Allocate(size) : NULL;
	if (mem == NULL)
	{
		return NULL;
	}
	switch (def->type)
	{
	case e_distanceJoint:
		return new (mem) b2DistanceJoint((b2DistanceJointDef*)def);
	case e_revoluteJoint:
		return new (mem) b2RevoluteJoint((b2RevoluteJointDef*)def);
	default:
		return new (mem) b2WeldJoint((b2WeldJointDef*)def);
	}
}

void b2Joint::Destroy(b2Joint* joint, b2BlockAllocator* allocator)
{
	int32 size = b2JointSize(joint->m_type);
	joint->~b2Joint();
	allocator->Free(joint, size);
}

b2Joint::b2Joint(const b2JointDef* def)
{
	b2Assert(def->bodyA != def->bodyB);

	m_type = def->type;
	m_prev = NULL;
	m_next = NULL;
	m_bodyA = def->bodyA;
	m_bodyB = def->bodyB;
	m_index = 0;
	m_collideConnected = def->collideConnected;
	m_islandFlag = false;

	m_edgeA.joint = NULL;
	m_edgeA.next = NULL;

	m_edgeB.joint = NULL;
	m_edgeB.next = NULL;
}

bool b2Joint::IsActive() const
{
	return m_bodyA->IsActive() && m_bodyB->IsActive();
}
