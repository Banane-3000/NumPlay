/* Altered for NumPlay's Hill Climb Racing (NumWorks calculator): only the distance, revolute and wheel joints are made */
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
#include <Box2D/Dynamics/Joints/b2WheelJoint.h>
#include <Box2D/Dynamics/Joints/b2RevoluteJoint.h>
#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/b2World.h>
#include <Box2D/Common/b2BlockAllocator.h>

#include <new>

b2Joint* b2Joint::Create(const b2JointDef* def, b2BlockAllocator* allocator)
{
	b2Joint* joint = NULL;

	switch (def->type)
	{
	case e_distanceJoint:
		{
			void* mem = allocator->Allocate(sizeof(b2DistanceJoint));
			joint = new (mem) b2DistanceJoint((b2DistanceJointDef*)def);
		}
		break;

	case e_revoluteJoint:
		{
			void* mem = allocator->Allocate(sizeof(b2RevoluteJoint));
			joint = new (mem) b2RevoluteJoint((b2RevoluteJointDef*)def);
		}
		break;

	case e_wheelJoint:
		{
			void* mem = allocator->Allocate(sizeof(b2WheelJoint));
			joint = new (mem) b2WheelJoint((b2WheelJointDef*)def);
		}
		break;

	default:
		b2Assert(false);
		break;
	}

	return joint;
}

void b2Joint::Destroy(b2Joint* joint, b2BlockAllocator* allocator)
{
	joint->~b2Joint();
	switch (joint->m_type)
	{
	case e_distanceJoint:
		allocator->Free(joint, sizeof(b2DistanceJoint));
		break;

	case e_revoluteJoint:
		allocator->Free(joint, sizeof(b2RevoluteJoint));
		break;

	case e_wheelJoint:
		allocator->Free(joint, sizeof(b2WheelJoint));
		break;
    
	default:
		b2Assert(false);
		break;
	}
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
	m_userData = def->userData;

	m_edgeA.joint = NULL;
	m_edgeA.other = NULL;
	m_edgeA.prev = NULL;
	m_edgeA.next = NULL;

	m_edgeB.joint = NULL;
	m_edgeB.other = NULL;
	m_edgeB.prev = NULL;
	m_edgeB.next = NULL;
}

bool b2Joint::IsActive() const
{
	return m_bodyA->IsActive() && m_bodyB->IsActive();
}
