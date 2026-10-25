/* Altered for NumPlay's Angry Birds (NumWorks calculator): always rigid (no frequency or damping: the game's welds have none); the bodies' island indices,
 * centres and inverse masses read from the bodies rather than copied; no dump */
/*
* Copyright (c) 2006-2011 Erin Catto http://www.box2d.org
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

#include <Box2D/Dynamics/Joints/b2WeldJoint.h>
#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/b2TimeStep.h>

// Point-to-point constraint
// C = p2 - p1
// Cdot = v2 - v1
//      = v2 + cross(w2, r2) - v1 - cross(w1, r1)
// J = [-I -r1_skew I r2_skew ]
// Identity used:
// w k % (rx i + ry j) = w * (-ry i + rx j)

// Angle constraint
// C = angle2 - angle1 - referenceAngle
// Cdot = w2 - w1
// J = [0 0 -1 0 0 1]
// K = invI1 + invI2

void b2WeldJointDef::Initialize(b2Body* bA, b2Body* bB, const b2Vec2& anchor)
{
	bodyA = bA;
	bodyB = bB;
	localAnchorA = bodyA->GetLocalPoint(anchor);
	localAnchorB = bodyB->GetLocalPoint(anchor);
	referenceAngle = bodyB->GetAngle() - bodyA->GetAngle();
}

b2WeldJoint::b2WeldJoint(const b2WeldJointDef* def)
: b2Joint(def)
{
	m_localAnchorA = def->localAnchorA;
	m_localAnchorB = def->localAnchorB;
	m_referenceAngle = def->referenceAngle;

	m_impulse.SetZero();
}

void b2WeldJoint::InitVelocityConstraints(const b2SolverData& data)
{

	b2Vec2 cA = data.positions[m_bodyA->m_islandIndex].c;
	float32 aA = data.positions[m_bodyA->m_islandIndex].a;
	b2Vec2 vA = data.velocities[m_bodyA->m_islandIndex].v;
	float32 wA = data.velocities[m_bodyA->m_islandIndex].w;

	b2Vec2 cB = data.positions[m_bodyB->m_islandIndex].c;
	float32 aB = data.positions[m_bodyB->m_islandIndex].a;
	b2Vec2 vB = data.velocities[m_bodyB->m_islandIndex].v;
	float32 wB = data.velocities[m_bodyB->m_islandIndex].w;

	b2Rot qA(aA), qB(aB);

	m_rA = b2Mul(qA, m_localAnchorA - m_bodyA->m_sweep.localCenter);
	m_rB = b2Mul(qB, m_localAnchorB - m_bodyB->m_sweep.localCenter);

	// J = [-I -r1_skew I r2_skew]
	//     [ 0       -1 0       1]
	// r_skew = [-ry; rx]

	// Matlab
	// K = [ mA+r1y^2*iA+mB+r2y^2*iB,  -r1y*iA*r1x-r2y*iB*r2x,          -r1y*iA-r2y*iB]
	//     [  -r1y*iA*r1x-r2y*iB*r2x, mA+r1x^2*iA+mB+r2x^2*iB,           r1x*iA+r2x*iB]
	//     [          -r1y*iA-r2y*iB,           r1x*iA+r2x*iB,                   iA+iB]

	float32 mA = m_bodyA->m_invMass, mB = m_bodyB->m_invMass;
	float32 iA = m_bodyA->m_invI, iB = m_bodyB->m_invI;

	b2Mat33 K;
	K.ex.x = mA + mB + m_rA.y * m_rA.y * iA + m_rB.y * m_rB.y * iB;
	K.ey.x = -m_rA.y * m_rA.x * iA - m_rB.y * m_rB.x * iB;
	K.ez.x = -m_rA.y * iA - m_rB.y * iB;
	K.ex.y = K.ey.x;
	K.ey.y = mA + mB + m_rA.x * m_rA.x * iA + m_rB.x * m_rB.x * iB;
	K.ez.y = m_rA.x * iA + m_rB.x * iB;
	K.ex.z = K.ez.x;
	K.ey.z = K.ez.y;
	K.ez.z = iA + iB;

	// (NumPlay: rigid, the game's welds have no frequency)
	b2Mat33 M;
	K.GetSymInverse33(&M);
	m_mass[0] = M.ex.x, m_mass[1] = M.ex.y, m_mass[2] = M.ex.z;
	m_mass[3] = M.ey.y, m_mass[4] = M.ey.z, m_mass[5] = M.ez.z;

	if (data.step.warmStarting)
	{
		// Scale impulses to support a variable time step.
		m_impulse *= data.step.dtRatio;

		b2Vec2 P(m_impulse.x, m_impulse.y);

		vA -= mA * P;
		wA -= iA * (b2Cross(m_rA, P) + m_impulse.z);

		vB += mB * P;
		wB += iB * (b2Cross(m_rB, P) + m_impulse.z);
	}
	else
	{
		m_impulse.SetZero();
	}

	data.velocities[m_bodyA->m_islandIndex].v = vA;
	data.velocities[m_bodyA->m_islandIndex].w = wA;
	data.velocities[m_bodyB->m_islandIndex].v = vB;
	data.velocities[m_bodyB->m_islandIndex].w = wB;
}

void b2WeldJoint::SolveVelocityConstraints(const b2SolverData& data)
{
	b2Vec2 vA = data.velocities[m_bodyA->m_islandIndex].v;
	float32 wA = data.velocities[m_bodyA->m_islandIndex].w;
	b2Vec2 vB = data.velocities[m_bodyB->m_islandIndex].v;
	float32 wB = data.velocities[m_bodyB->m_islandIndex].w;

	float32 mA = m_bodyA->m_invMass, mB = m_bodyB->m_invMass;
	float32 iA = m_bodyA->m_invI, iB = m_bodyB->m_invI;

	b2Vec2 Cdot1 = vB + b2Cross(wB, m_rB) - vA - b2Cross(wA, m_rA);
	float32 Cdot2 = wB - wA;
	b2Vec3 Cdot(Cdot1.x, Cdot1.y, Cdot2);

	b2Mat33 M(b2Vec3(m_mass[0], m_mass[1], m_mass[2]), b2Vec3(m_mass[1], m_mass[3], m_mass[4]),
			  b2Vec3(m_mass[2], m_mass[4], m_mass[5]));
	b2Vec3 impulse = -b2Mul(M, Cdot);
	m_impulse += impulse;

	b2Vec2 P(impulse.x, impulse.y);

	vA -= mA * P;
	wA -= iA * (b2Cross(m_rA, P) + impulse.z);

	vB += mB * P;
	wB += iB * (b2Cross(m_rB, P) + impulse.z);

	data.velocities[m_bodyA->m_islandIndex].v = vA;
	data.velocities[m_bodyA->m_islandIndex].w = wA;
	data.velocities[m_bodyB->m_islandIndex].v = vB;
	data.velocities[m_bodyB->m_islandIndex].w = wB;
}

bool b2WeldJoint::SolvePositionConstraints(const b2SolverData& data)
{
	b2Vec2 cA = data.positions[m_bodyA->m_islandIndex].c;
	float32 aA = data.positions[m_bodyA->m_islandIndex].a;
	b2Vec2 cB = data.positions[m_bodyB->m_islandIndex].c;
	float32 aB = data.positions[m_bodyB->m_islandIndex].a;

	b2Rot qA(aA), qB(aB);

	float32 mA = m_bodyA->m_invMass, mB = m_bodyB->m_invMass;
	float32 iA = m_bodyA->m_invI, iB = m_bodyB->m_invI;

	b2Vec2 rA = b2Mul(qA, m_localAnchorA - m_bodyA->m_sweep.localCenter);
	b2Vec2 rB = b2Mul(qB, m_localAnchorB - m_bodyB->m_sweep.localCenter);

	float32 positionError, angularError;

	b2Mat33 K;
	K.ex.x = mA + mB + rA.y * rA.y * iA + rB.y * rB.y * iB;
	K.ey.x = -rA.y * rA.x * iA - rB.y * rB.x * iB;
	K.ez.x = -rA.y * iA - rB.y * iB;
	K.ex.y = K.ey.x;
	K.ey.y = mA + mB + rA.x * rA.x * iA + rB.x * rB.x * iB;
	K.ez.y = rA.x * iA + rB.x * iB;
	K.ex.z = K.ez.x;
	K.ey.z = K.ez.y;
	K.ez.z = iA + iB;

	b2Vec2 C1 =  cB + rB - cA - rA;
	float32 C2 = aB - aA - m_referenceAngle;

	positionError = C1.Length();
	angularError = b2Abs(C2);

	b2Vec3 C(C1.x, C1.y, C2);

	b2Vec3 impulse = -K.Solve33(C);
	b2Vec2 P(impulse.x, impulse.y);

	cA -= mA * P;
	aA -= iA * (b2Cross(rA, P) + impulse.z);

	cB += mB * P;
	aB += iB * (b2Cross(rB, P) + impulse.z);

	data.positions[m_bodyA->m_islandIndex].c = cA;
	data.positions[m_bodyA->m_islandIndex].a = aA;
	data.positions[m_bodyB->m_islandIndex].c = cB;
	data.positions[m_bodyB->m_islandIndex].a = aB;

	return positionError <= b2_linearSlop && angularError <= b2_angularSlop;
}

b2Vec2 b2WeldJoint::GetAnchorA() const
{
	return m_bodyA->GetWorldPoint(m_localAnchorA);
}

b2Vec2 b2WeldJoint::GetAnchorB() const
{
	return m_bodyB->GetWorldPoint(m_localAnchorB);
}

b2Vec2 b2WeldJoint::GetReactionForce(float32 inv_dt) const
{
	b2Vec2 P(m_impulse.x, m_impulse.y);
	return inv_dt * P;
}

float32 b2WeldJoint::GetReactionTorque(float32 inv_dt) const
{
	return inv_dt * m_impulse.z;
}
