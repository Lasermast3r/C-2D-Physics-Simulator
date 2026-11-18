#pragma once

#include <box2d/box2d.h>

// 1 Meter = 30 Pixels
const float SCALE = 30.0f;

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void Step();

    b2Body* CreateBox(float pixelX, float pixelY, float pixelWidth, float pixelHeight, bool isDynamic);

    // --- MOUSE JOINT FUNCTIONS ---
    // Make sure these three lines are exactly here
    b2MouseJoint* CreateMouseJoint(b2Body* body, float pixelX, float pixelY);
    void UpdateMouseJoint(b2MouseJoint* joint, float pixelX, float pixelY);
    void DestroyMouseJoint(b2MouseJoint* joint);
    b2DistanceJoint* CreateSpring(b2Body* bodyA, b2Body* bodyB);
    void DestroySpring(b2DistanceJoint* joint);
    b2World world;

private:
    b2Body* groundBody;
};