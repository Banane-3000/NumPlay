/* Altered for NumPlay's Angry Birds (NumWorks calculator): fixtures share the shape they are given (the game keeps
 * its shapes) instead of copying it, keep their proxy themselves, and have no filter data or sensors */
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

#include <Box2D/Dynamics/b2Fixture.h>
#include <Box2D/Dynamics/Contacts/b2Contact.h>
#include <Box2D/Dynamics/b2World.h>
#include <Box2D/Collision/Shapes/b2CircleShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>
#include <Box2D/Collision/b2BroadPhase.h>
#include <Box2D/Collision/b2Collision.h>
#include <Box2D/Common/b2BlockAllocator.h>

b2Fixture::b2Fixture()
{
	m_userData = NULL;
	m_body = NULL;
	m_next = NULL;
	m_proxyId = b2BroadPhase::e_nullProxy;
	m_shape = NULL;
	m_density = 0.0f;
}

void b2Fixture::Create(b2BlockAllocator* allocator, b2Body* body, const b2FixtureDef* def)
{
	m_userData = def->userData;
	m_friction = def->friction;
	m_restitution = def->restitution;

	m_body = body;
	m_next = NULL;

	m_shape = (b2Shape*)def->shape;	// (shared, not copied)

	// (circles and polygons have one child, its proxy made by CreateProxies)
	(void)allocator;
	m_proxyId = b2BroadPhase::e_nullProxy;

	m_density = def->density;
}

void b2Fixture::Destroy(b2BlockAllocator* allocator)
{
	// The proxies must be destroyed before calling this.
	b2Assert(m_proxyId == b2BroadPhase::e_nullProxy);
	(void)allocator;

	// (the shape is the game's: not freed)
	m_shape = NULL;
}

void b2Fixture::CreateProxies(b2BroadPhase* broadPhase, const b2Transform& xf)
{
	b2Assert(m_proxyId == b2BroadPhase::e_nullProxy);

	// Create the proxy in the broad-phase.
	b2AABB aabb;
	m_shape->ComputeAABB(&aabb, xf, 0);
	m_proxyId = broadPhase->CreateProxy(aabb, this);
}

void b2Fixture::DestroyProxies(b2BroadPhase* broadPhase)
{
	// Destroy the proxy in the broad-phase.
	if (m_proxyId != b2BroadPhase::e_nullProxy)
	{
		broadPhase->DestroyProxy(m_proxyId);
	}
	m_proxyId = b2BroadPhase::e_nullProxy;
}

void b2Fixture::Synchronize(b2BroadPhase* broadPhase, const b2Transform& transform1, const b2Transform& transform2)
{
	if (m_proxyId == b2BroadPhase::e_nullProxy)
	{
		return;
	}

	// Compute an AABB that covers the swept shape (may miss some rotation effect).
	b2AABB aabb1, aabb2, aabb;
	m_shape->ComputeAABB(&aabb1, transform1, 0);
	m_shape->ComputeAABB(&aabb2, transform2, 0);

	aabb.Combine(aabb1, aabb2);

	b2Vec2 displacement = transform2.p - transform1.p;

	broadPhase->MoveProxy(m_proxyId, aabb, displacement);
}


void b2Fixture::Refilter()
{
	if (m_body == NULL)
	{
		return;
	}

	// Flag associated contacts for filtering.
	b2ContactEdge* edge = m_body->GetContactList();
	while (edge)
	{
		b2Contact* contact = edge->contact;
		b2Fixture* fixtureA = contact->GetFixtureA();
		b2Fixture* fixtureB = contact->GetFixtureB();
		if (fixtureA == this || fixtureB == this)
		{
			contact->FlagForFiltering();
		}

		edge = edge->next;
	}

	b2World* world = m_body->GetWorld();

	if (world == NULL)
	{
		return;
	}

	// Touch each proxy so that new pairs may be created
	b2BroadPhase* broadPhase = &world->m_contactManager.m_broadPhase;
	if (m_proxyId != b2BroadPhase::e_nullProxy)
	{
		broadPhase->TouchProxy(m_proxyId);
	}
}


