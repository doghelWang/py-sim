#pragma once
#include "amr/AppModel.hpp"
#include "gui/IView.hpp"
#include <string>
#include <vector>

namespace gui {

struct Point3D {
  float x, y, z;
};
struct Point2D {
  float x, y;
  float scale;
  bool visible;
};

struct Camera {
  float x = 0, y = 0, z = 0; // Look-at target
  float pitch = -0.785f;
  float yaw = 0.0f;
  float dist = 600.0f; // Orbit distance
};

class SimulatorView : public IView {
public:
  SimulatorView();
  void Render() override;
  const char *GetName() const override { return "Simulator"; }

private:
  void Draw3DViewport();
  void Draw2DCanvas();
  Point2D Project3D(const Point3D &p, const ImVec2 &screen_center, float scale);

  Camera m_cam;
  bool m_cam_follow = true;
};

} // namespace gui
