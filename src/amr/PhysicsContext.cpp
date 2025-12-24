#include "amr/PhysicsContext.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace amr {

PhysicsContext::PhysicsContext() {
    odom_ = {0, 0, 0, 0, 0, 0};
    // Default Map
    AddObstacle(100, 100, 20, 200); 
    AddObstacle(300, 100, 200, 20); 
    AddObstacle(400, 300, 50, 50); 
}

PhysicsContext::Derivative PhysicsContext::Evaluate(const State &initial, float dt, const Derivative &d, const Twist &ctrl) {
    State state;
    state.x = initial.x + d.dx * dt;
    state.y = initial.y + d.dy * dt;
    state.theta = initial.theta + d.dtheta * dt;
    // Velocity is commanded directly in this kinematic model, but we treat it as state for RK4 flow
    // Ideally for kinematic, RK4 is overkill if V is constant, but if V changes or for drift...
    // Actually for pure kinematic omni drive:
    // dx/dt = vx_global
    // dy/dt = vy_global
    // dtheta/dt = omega
    
    // Convert local control to global frame at current estimated theta
    float cos_t = std::cos(state.theta);
    float sin_t = std::sin(state.theta);
    
    // 100.0f scale factor preserved from legacy
    float vx_local = ctrl.linear_x * 100.0f;
    float vy_local = ctrl.linear_y * 100.0f;

    Derivative output;
    output.dx = vx_local * cos_t - vy_local * sin_t;
    output.dy = vx_local * sin_t + vy_local * cos_t;
    output.dtheta = ctrl.angular_z;
    output.dvx = 0; // Acceleration not modeled
    output.dvy = 0;
    output.domega = 0;
    
    return output;
}

PhysicsContext::State PhysicsContext::Integrate(const State &state, float dt, const Twist &ctrl) {
    Derivative a = Evaluate(state, 0.0f, Derivative(), ctrl);
    Derivative b = Evaluate(state, dt * 0.5f, a, ctrl);
    Derivative c = Evaluate(state, dt * 0.5f, b, ctrl);
    Derivative d = Evaluate(state, dt, c, ctrl);

    State output;
    output.x = state.x + (a.dx + 2.0f * b.dx + 2.0f * c.dx + d.dx) * (dt / 6.0f);
    output.y = state.y + (a.dy + 2.0f * b.dy + 2.0f * c.dy + d.dy) * (dt / 6.0f);
    output.theta = state.theta + (a.dtheta + 2.0f * b.dtheta + 2.0f * c.dtheta + d.dtheta) * (dt / 6.0f);
    
    // Velocity is just current command
    output.vx = ctrl.linear_x * 100.0f;
    output.vy = ctrl.linear_y * 100.0f;
    output.omega = ctrl.angular_z;

    return output;
}

void PhysicsContext::UpdateChassis(const Twist &cmd, float dt) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (dt > 0.1f) dt = 0.1f;
    if (dt < 1e-6f) return;

    State current;
    current.x = (float)odom_.x;
    current.y = (float)odom_.y;
    current.theta = (float)odom_.theta;
    current.vx = (float)odom_.vx;
    current.vy = (float)odom_.vy;
    current.omega = (float)odom_.omega;

    State next = Integrate(current, dt, cmd);

    // Normalize Theta
    if (next.theta > M_PI) next.theta -= 2 * M_PI;
    if (next.theta < -M_PI) next.theta += 2 * M_PI;

    odom_.x = next.x;
    odom_.y = next.y;
    odom_.theta = next.theta;
    odom_.vx = next.vx;
    odom_.vy = next.vy;
    odom_.omega = next.omega;
}

void PhysicsContext::Update(float dt) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // Particle Physics
    for (auto &p : particles_) {
        p.x += p.vx;
        p.y += p.vy;
        p.life -= dt;
    }

    // Ghetto remove_if
    int w_idx = 0;
    for (int r_idx = 0; r_idx < (int)particles_.size(); ++r_idx) {
        if (particles_[r_idx].life > 0) {
            if (w_idx != r_idx) {
                particles_[w_idx] = particles_[r_idx];
            }
            w_idx++;
        }
    }
    particles_.resize(w_idx);

    if (shake_timer_ > 0) {
        shake_timer_ -= dt;
        if (shake_timer_ < 0) shake_timer_ = 0;
    }
}

Odometry PhysicsContext::GetOdometry() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return odom_;
}

void PhysicsContext::SetOdometry(const Odometry& odom) {
    std::lock_guard<std::mutex> lock(mtx_);
    odom_ = odom;
}

void PhysicsContext::ResetOdometry() {
    std::lock_guard<std::mutex> lock(mtx_);
    odom_ = Odometry{};
}

void PhysicsContext::AddObstacle(float x, float y, float w, float h) {
    std::lock_guard<std::mutex> lock(mtx_);
    obstacles_.push_back({x, y, w, h});
}

const std::vector<Rect>& PhysicsContext::GetObstacles() const {
    return obstacles_;
}

void PhysicsContext::ResetObstacles() {
    std::lock_guard<std::mutex> lock(mtx_);
    obstacles_.clear();
}

void PhysicsContext::SpawnParticles(float x, float y, int count, int color) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.x = x; 
        p.y = y;
        p.vx = ((rand() % 100) / 10.0f) - 5.0f;
        p.vy = ((rand() % 100) / 10.0f) - 5.0f;
        p.life = 1.0f;
        p.color = static_cast<unsigned int>(color);
        particles_.push_back(p);
    }
}

std::vector<Particle>& PhysicsContext::GetParticles() {
    return particles_;
}

void PhysicsContext::SetShakeTimer(float force) {
    std::lock_guard<std::mutex> lock(mtx_);
    shake_timer_ = force;
}

float PhysicsContext::GetShakeTimer() const {
    return shake_timer_;
}

// Raycasting Helper
static float IntersectRayRect(float rx, float ry, float dx, float dy, const Rect &rect) {
     float min_dist = std::numeric_limits<float>::max();
     bool hit = false;
     auto check_line = [&](float x1, float y1, float x2, float y2) {
        float sx = x2 - x1;
        float sy = y2 - y1;
        float denom = dx * sy - dy * sx;
        if (std::abs(denom) < 1e-6) return;
        float t = ((x1 - rx) * sy - (y1 - ry) * sx) / denom;
        float u = ((x1 - rx) * dy - (y1 - ry) * dx) / denom;
        if (t > 0 && u >= 0 && u <= 1) {
            if (t < min_dist) { min_dist = t; hit = true; }
        }
     };
     check_line(rect.x, rect.y, rect.x + rect.w, rect.y);
     check_line(rect.x, rect.y, rect.x, rect.y + rect.h);
     check_line(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h);
     check_line(rect.x, rect.y + rect.h, rect.x + rect.w, rect.y + rect.h);
     return hit ? min_dist : -1.0f;
}

LidarScan PhysicsContext::RayCast(const Odometry &pose) const {
    std::lock_guard<std::mutex> lock(mtx_);
    LidarScan scan;
    const int rays = 180; // Optimized from 360
    scan.angle_min = 0;
    scan.angle_max = 2 * M_PI;
    scan.angle_increment = (2 * M_PI) / rays;
    scan.range_min = 1.0f;
    scan.range_max = 1000.0f;
    scan.ranges.resize(rays, scan.range_max);

    for (int i = 0; i < rays; ++i) {
         float angle = pose.theta + (i * scan.angle_increment);
         float dx = std::cos(angle);
         float dy = std::sin(angle);
         float closest = scan.range_max;
         for (const auto &rect : obstacles_) {
             float dist = IntersectRayRect(pose.x, pose.y, dx, dy, rect);
             if (dist > 0 && dist < closest) closest = dist;
         }
         scan.ranges[i] = closest;
    }
    return scan;
}

} // namespace amr
