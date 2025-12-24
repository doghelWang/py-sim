#include "amr/PhysicsContext.hpp"
#include <gtest/gtest.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper to check circular closeness
void ExpectThetaNear(float a, float b, float tol = 1e-4f) {
    float diff = std::abs(a - b);
    while (diff > M_PI) diff -= 2 * M_PI;
    while (diff < -M_PI) diff += 2 * M_PI;
    EXPECT_NEAR(diff, 0.0f, tol);
}

TEST(PhysicsRK4, BasicMovement) {
    amr::PhysicsContext phys;
    phys.ResetOdometry();

    // 1 m/s straight
    amr::Twist cmd;
    cmd.linear_x = 1.0f; 
    cmd.linear_y = 0.0f;
    cmd.angular_z = 0.0f;

    // Simulate 1 second in 0.01s steps
    for(int i=0; i<100; ++i) {
        phys.UpdateChassis(cmd, 0.01f);
    }

    // Expected: x = 1.0 * 100cm = 100.0f
    auto odom = phys.GetOdometry();
    EXPECT_NEAR(odom.x, 100.0f, 0.1f);
    EXPECT_NEAR(odom.y, 0.0f, 0.1f);
    ExpectThetaNear(odom.theta, 0.0f);
}

TEST(PhysicsRK4, CircularMotion) {
    amr::PhysicsContext phys;
    phys.ResetOdometry();

    // V = 1 m/s (100 cm/s), Omega = PI/2 rad/s
    // Radius = V/Omega = 100 / (PI/2) = 200/PI ~= 63.66 cm
    // Quarter circle in 1 second
    amr::Twist cmd;
    cmd.linear_x = 1.0f;
    cmd.linear_y = 0.0f;
    cmd.angular_z = M_PI / 2.0f;

    // Simulate 1 second
    for(int i=0; i<100; ++i) {
        phys.UpdateChassis(cmd, 0.01f);
    }

    auto odom = phys.GetOdometry();
    
    // Expected position after 90 degrees turn with V=1m/s
    // x = R * sin(theta) = 63.66 * 1 = 63.66
    // y = R * (1 - cos(theta)) = 63.66 * 1 = 63.66
    // Note: Our coord system might result in different x/y mapping depending on initial theta=0
    // With theta=0 facing X+:
    // dx/dt = v * cos(theta), dy/dt = v * sin(theta)
    
    // Actually, integrate:
    // x(t) = (v/w) * sin(wt)
    // y(t) = (v/w) * (1 - cos(wt))
    // t=1, w=PI/2
    // x = (100 * 2/PI) * 1 = 63.66
    // y = (100 * 2/PI) * (1 - 0) = 63.66
    
    EXPECT_NEAR(odom.x, 63.66f, 0.5f); // RK4 should be very close
    EXPECT_NEAR(odom.y, 63.66f, 0.5f);
    ExpectThetaNear(odom.theta, M_PI / 2.0f);
}

TEST(PhysicsRK4, RK4vsEulerDrift) {
    // Compare accumulation error? 
    // Just verify high precision on a full circle
    amr::PhysicsContext phys;
    phys.ResetOdometry();

    // Full Circle: 4 seconds at PI/2
    amr::Twist cmd;
    cmd.linear_x = 1.0f;
    cmd.angular_z = M_PI / 2.0f;

    for(int i=0; i<400; ++i) {
        phys.UpdateChassis(cmd, 0.01f);
    }

    auto odom = phys.GetOdometry();
    
    // Should be back at 0,0
    EXPECT_NEAR(odom.x, 0.0f, 0.5f); 
    EXPECT_NEAR(odom.y, 0.0f, 0.5f);
    ExpectThetaNear(odom.theta, 0.0f); // 2*PI ~= 0
}
