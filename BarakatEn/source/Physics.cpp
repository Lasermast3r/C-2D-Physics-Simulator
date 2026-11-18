#include "Physics.hpp"

PhysicsWorld::PhysicsWorld() : world(b2Vec2(0.0f, 13.2f)) {
    // Create the dummy static body required for mouse joints
    b2BodyDef bodyDef;
    groundBody = world.CreateBody(&bodyDef);
}

PhysicsWorld::~PhysicsWorld() {
}

void PhysicsWorld::Step() {
    float timeStep = 1.0f / 60.0f;
    int32 velocityIterations = 8;
    int32 positionIterations = 3;
    world.Step(timeStep, velocityIterations, positionIterations);
}

b2Body* PhysicsWorld::CreateBox(float pixelX, float pixelY, float pixelWidth, float pixelHeight, bool isDynamic) {
    b2BodyDef bodyDef;
    bodyDef.type = isDynamic ? b2_dynamicBody : b2_staticBody;
    bodyDef.position.Set(pixelX / SCALE, pixelY / SCALE);

    // Prevent fast objects from tunneling through walls
    if (isDynamic) bodyDef.bullet = true;

    b2Body* body = world.CreateBody(&bodyDef);

    b2PolygonShape boxShape;
    boxShape.SetAsBox((pixelWidth / 2.0f) / SCALE, (pixelHeight / 2.0f) / SCALE);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &boxShape;
    fixtureDef.density = 1.0f;
    fixtureDef.friction = 1.3f;
    fixtureDef.restitution = 0.5f;

    body->CreateFixture(&fixtureDef);
    return body;
}

// --- MOUSE JOINT LOGIC ---

b2MouseJoint* PhysicsWorld::CreateMouseJoint(b2Body* body, float pixelX, float pixelY) {
    b2MouseJointDef jointDef;
    jointDef.bodyA = groundBody;
    jointDef.bodyB = body;
    jointDef.target.Set(pixelX / SCALE, pixelY / SCALE);

    jointDef.maxForce = 1000.0f * body->GetMass();
    jointDef.stiffness = 2000.0f;
    jointDef.damping = 100.0f;

    return (b2MouseJoint*)world.CreateJoint(&jointDef);
}

void PhysicsWorld::UpdateMouseJoint(b2MouseJoint* joint, float pixelX, float pixelY) {
    if (joint) {
        joint->SetTarget(b2Vec2(pixelX / SCALE, pixelY / SCALE));
        joint->GetBodyB()->SetAwake(true);
    }
}

void PhysicsWorld::DestroyMouseJoint(b2MouseJoint* joint) {
    if (joint) {
        world.DestroyJoint(joint);
    }
}
b2DistanceJoint* PhysicsWorld::CreateSpring(b2Body* bodyA, b2Body* bodyB) {
    b2DistanceJointDef def;
    def.Initialize(bodyA, bodyB, bodyA->GetWorldCenter(), bodyB->GetWorldCenter());

    def.length = 100.0f; // Pull towards the exact center
    def.stiffness = 100.0f; // Spring strength
    def.damping = 0.5f;     // Bounciness (0 = bouncy, 1 = stiff)

    return (b2DistanceJoint*)world.CreateJoint(&def);
}

void PhysicsWorld::DestroySpring(b2DistanceJoint* joint) {
    if (joint) {
        world.DestroyJoint(joint);
    }
}