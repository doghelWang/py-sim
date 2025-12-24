#pragma once

#include "ServiceContext.hpp"
#include "Types.hpp"
#include "VehicleTypes.hpp"
#include <mutex>
#include <vector>

namespace amr {


class PhysicsContext : public IService {
public:
    PhysicsContext();
    ~PhysicsContext() override = default;

    void Update(float dt);

    // Kinematics
    void UpdateChassis(const Twist &cmd, float dt);
    Odometry GetOdometry() const;
    void SetOdometry(const Odometry& odom);
    void ResetOdometry();

    // Map / Obstacles
    void AddObstacle(float x, float y, float w, float h);
    const std::vector<Rect> &GetObstacles() const;
    void ResetObstacles();

    // Sensors
    LidarScan RayCast(const Odometry &pose) const;

    // Visual/Particles (Physics side)
    void SpawnParticles(float x, float y, int count, int color);
    std::vector<Particle>& GetParticles(); 
    
    // Effects
    void SetShakeTimer(float force);
    float GetShakeTimer() const;

private:
    struct State {
        float x, y, theta;
        float vx, vy, omega;
    };

    struct Derivative {
        float dx, dy, dtheta;
        float dvx, dvy, domega;
    };

    Derivative Evaluate(const State &initial, float dt, const Derivative &d, const Twist &ctrl);
    State Integrate(const State &state, float dt, const Twist &ctrl);

    mutable std::mutex mtx_;
    Odometry odom_;
    std::vector<Rect> obstacles_;
    std::vector<Particle> particles_;
    float shake_timer_ = 0.0f;
};

} // namespace amr
