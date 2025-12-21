#include "gui/SimulatorView.hpp"
#include "amr/SimulatorCore.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace gui {

static amr::AppModel &Model() { return amr::AppModel::Instance(); }

SimulatorView::SimulatorView() {
  m_cam.x = 0;
  m_cam.y = 0;
  m_cam.z = 0;
  m_cam.pitch = -0.785398f;
  m_cam.yaw = 0.0f;
  m_cam.dist = 1000.0f;
}

Point2D SimulatorView::Project3D(const Point3D &p, const ImVec2 &screen_center,
                                 float scale) {
  // Orbit Camera: Calculate camera pos relative to target (m_cam.x, y, z)
  float cp_x = m_cam.x + m_cam.dist * cos(m_cam.pitch) * cos(m_cam.yaw);
  float cp_y = m_cam.y + m_cam.dist * cos(m_cam.pitch) * sin(m_cam.yaw);
  float cp_z = m_cam.z + m_cam.dist * sin(-m_cam.pitch);

  float dx = p.x - cp_x;
  float dy = p.y - cp_y;
  float dz = p.z - cp_z;

  // View Matrix approach (simplified)
  float cos_y = cos(-m_cam.yaw - 1.57f);
  float sin_y = sin(-m_cam.yaw - 1.57f);
  float rx = dx * cos_y - dy * sin_y;
  float ry = dx * sin_y + dy * cos_y;
  float rz = dz;

  float cos_p = cos(-m_cam.pitch);
  float sin_p = sin(-m_cam.pitch);
  float y_cam = ry * sin_p + rz * cos_p;
  float z_cam = ry * cos_p - rz * sin_p;

  if (z_cam < 1.0f)
    return {0, 0, 0, false};
  float perspective = scale / z_cam;

  return {screen_center.x + rx * perspective,
          screen_center.y - y_cam * perspective, perspective, true};
}

void SimulatorView::Render() {
  ImVec2 avail = ImGui::GetContentRegionAvail();

  ImGui::BeginChild("RobotView", ImVec2(0, avail.y * 0.6f), true);
  Draw3DViewport();
  ImGui::EndChild();

  ImGui::BeginChild("GameView", ImVec2(0, 0), true);
  Draw2DCanvas();
  ImGui::EndChild();
}

void SimulatorView::Draw3DViewport() {
  ImGui::BeginChild("View3D", ImVec2(0, 0), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);
  ImDrawList *draw = ImGui::GetWindowDrawList();
  ImVec2 p_min = ImGui::GetWindowPos();
  ImVec2 p_max = ImVec2(p_min.x + ImGui::GetWindowWidth(),
                        p_min.y + ImGui::GetWindowHeight());

  // Input Handling for Orbit Camera
  if (ImGui::IsWindowHovered()) {
    ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
      m_cam.yaw -= io.MouseDelta.x * 0.01f;
      m_cam.pitch -= io.MouseDelta.y * 0.01f;
      m_cam.pitch = std::clamp(m_cam.pitch, -1.5f, -0.1f);
      m_cam_follow = false;
    }
    m_cam.dist -= io.MouseWheel * 20.0f;
    m_cam.dist = std::clamp(m_cam.dist, 50.0f, 3000.0f);
  }

  ImVec2 center =
      ImVec2((p_min.x + p_max.x) * 0.5f, (p_min.y + p_max.y) * 0.5f);
  draw->AddRectFilled(p_min, p_max, IM_COL32(30, 30, 30, 255));

  float dt = ImGui::GetIO().DeltaTime;
  auto odom = amr::SimulatorCore::Instance().GetOdometry();
  if (m_cam_follow) {
    float l = 1.0f - std::exp(-5.0f * dt);
    m_cam.x += ((float)odom.x - m_cam.x) * l;
    m_cam.y += ((float)odom.y - m_cam.y) * l;
    m_cam.z = 20.0f; // Look at deck height
  }

  float fScale = 500.0f;
  auto draw_grid = [&](float step, ImU32 col) {
    float sx = std::floor((m_cam.x - 3000) / step) * step;
    float ex = std::floor((m_cam.x + 3000) / step) * step;
    float sy = std::floor((m_cam.y - 3000) / step) * step;
    float ey = std::floor((m_cam.y + 3000) / step) * step;
    for (float x = sx; x <= ex; x += step) {
      Point2D p1 = Project3D({x, sy, 0}, center, fScale),
              p2 = Project3D({x, ey, 0}, center, fScale);
      if (p1.visible && p2.visible)
        draw->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), col);
    }
    for (float y = sy; y <= ey; y += step) {
      Point2D p1 = Project3D({sx, y, 0}, center, fScale),
              p2 = Project3D({ex, y, 0}, center, fScale);
      if (p1.visible && p2.visible)
        draw->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), col);
    }
  };
  draw_grid(100.0f, IM_COL32(50, 50, 55, 255));
  draw_grid(500.0f, IM_COL32(80, 80, 90, 255));

  // Obstacles
  for (const auto &w : amr::SimulatorCore::Instance().GetObstacles()) {
    float wx = w.x, wy = w.y, ww = w.w, wh = w.h, wz = 50.0f;
    Point2D b[4] = {Project3D({wx, wy, 0}, center, fScale),
                    Project3D({wx + ww, wy, 0}, center, fScale),
                    Project3D({wx + ww, wy + wh, 0}, center, fScale),
                    Project3D({wx, wy + wh, 0}, center, fScale)};
    Point2D t[4] = {Project3D({wx, wy, wz}, center, fScale),
                    Project3D({wx + ww, wy, wz}, center, fScale),
                    Project3D({wx + ww, wy + wh, wz}, center, fScale),
                    Project3D({wx, wy + wh, wz}, center, fScale)};
    if (b[0].visible && b[2].visible && t[0].visible && t[2].visible) {
      draw->AddQuadFilled(ImVec2(t[0].x, t[0].y), ImVec2(t[1].x, t[1].y),
                          ImVec2(t[2].x, t[2].y), ImVec2(t[3].x, t[3].y),
                          IM_COL32(100, 100, 120, 150));
      for (int i = 0; i < 4; ++i)
        draw->AddLine(ImVec2(b[i].x, b[i].y), ImVec2(t[i].x, t[i].y),
                      IM_COL32(150, 150, 180, 255));
    }
  }

  // AGV Rendering
  float rx = (float)odom.x, ry = (float)odom.y, sz = 25.0f;
  float ct = cos(odom.theta), st = sin(odom.theta);
  auto rot = [&](float x, float y, float z = 0) {
    return Point3D{rx + x * ct - y * st, ry + x * st + y * ct, z};
  };

  Point2D rb[4] = {Project3D(rot(-sz, -sz, 0), center, fScale),
                   Project3D(rot(sz, -sz, 0), center, fScale),
                   Project3D(rot(sz, sz, 0), center, fScale),
                   Project3D(rot(-sz, sz, 0), center, fScale)};

  auto agv_type = Model().GetAgvType();
  ImU32 chassis_col = IM_COL32(100, 100, 110, 255);
  float chassis_h = 20.0f;
  if (agv_type == amr::AgvType::FORKER) {
    chassis_col = IM_COL32(200, 200, 210, 255);
    chassis_h = 12.0f;
  } else if (agv_type == amr::AgvType::CTU) {
    chassis_col = IM_COL32(230, 230, 240, 255);
    chassis_h = 25.0f;
  }

  Point2D rt[4] = {Project3D(rot(-sz, -sz, chassis_h), center, fScale),
                   Project3D(rot(sz, -sz, chassis_h), center, fScale),
                   Project3D(rot(sz, sz, chassis_h), center, fScale),
                   Project3D(rot(-sz, sz, chassis_h), center, fScale)};

  if (rb[0].visible && rb[2].visible && rt[0].visible && rt[2].visible) {
    draw->AddQuadFilled(ImVec2(rt[0].x, rt[0].y), ImVec2(rt[1].x, rt[1].y),
                        ImVec2(rt[2].x, rt[2].y), ImVec2(rt[3].x, rt[3].y),
                        chassis_col);
    for (int i = 0; i < 4; ++i)
      draw->AddLine(ImVec2(rb[i].x, rb[i].y), ImVec2(rt[i].x, rt[i].y),
                    IM_COL32(60, 60, 60, 255));
  }

  // IO Feedback (LEDs & Sensors)
  for (int i = 0; i < 4; ++i) {
    // DO: Rear LED indicators (Amber)
    if (Model().GetDO(i)) {
      Point2D p =
          Project3D(rot(-sz - 2, -10 + i * 10, chassis_h - 5), center, fScale);
      if (p.visible)
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f * p.scale,
                              IM_COL32(255, 180, 0, 255));
    }
    // DI: Front Sensors (Green)
    if (Model().GetDI(i)) {
      Point2D p = Project3D(rot(sz + 2, -10 + i * 10, 5), center, fScale);
      if (p.visible) {
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f * p.scale,
                              IM_COL32(0, 255, 0, 255));
        // Sensor Beam
        Point2D p_end =
            Project3D(rot(sz + 40, -10 + i * 10, 5), center, fScale);
        if (p_end.visible)
          draw->AddLine(ImVec2(p.x, p.y), ImVec2(p_end.x, p_end.y),
                        IM_COL32(0, 255, 0, 100), 2.0f);
      }
    }
  }

  // Direction Indicator
  Point2D h = Project3D(rot(sz * 1.2f, 0, chassis_h + 1), center, fScale),
          l = Project3D(rot(sz * 0.5f, -sz * 0.5f, chassis_h + 1), center,
                        fScale),
          r = Project3D(rot(sz * 0.5f, sz * 0.5f, chassis_h + 1), center,
                        fScale);
  if (h.visible)
    draw->AddTriangleFilled(ImVec2(h.x, h.y), ImVec2(l.x, l.y),
                            ImVec2(r.x, r.y), IM_COL32(255, 255, 255, 200));

  // Specialized Rendering
  if (agv_type == amr::AgvType::FORKER) {
    Point3D m_tl = rot(sz - 5, -sz * 0.8f, 120),
            m_tr = rot(sz - 5, sz * 0.8f, 120);
    Point2D p_tl = Project3D(m_tl, center, fScale),
            p_tr = Project3D(m_tr, center, fScale);
    Point2D p_bl = Project3D(rot(sz - 5, -sz * 0.8f, 20), center, fScale);
    Point2D p_br = Project3D(rot(sz - 5, sz * 0.8f, 20), center, fScale);
    if (p_tl.visible && p_tr.visible) {
      draw->AddLine(ImVec2(p_bl.x, p_bl.y), ImVec2(p_tl.x, p_tl.y),
                    IM_COL32(80, 80, 85, 255), 6.0f);
      draw->AddLine(ImVec2(p_br.x, p_br.y), ImVec2(p_tr.x, p_tr.y),
                    IM_COL32(80, 80, 85, 255), 6.0f);
      draw->AddLine(ImVec2(p_tl.x, p_tl.y), ImVec2(p_tr.x, p_tr.y),
                    IM_COL32(100, 100, 110, 255), 4.0f);
    }
    float lift = Model().GetAxisPos(3) * 0.8f;
    for (float s : {-sz * 0.5f, sz * 0.5f}) {
      Point2D fp1 = Project3D(rot(sz + 5, s, 30 + lift), center, fScale);
      Point2D fp2 = Project3D(rot(sz + 55, s, 28 + lift), center, fScale);
      if (fp1.visible && fp2.visible)
        draw->AddLine(ImVec2(fp1.x, fp1.y), ImVec2(fp2.x, fp2.y),
                      IM_COL32(40, 40, 40, 255), 8.0f * fp1.scale);
    }
  } else if (agv_type == amr::AgvType::CTU) {
    float ch = 180.0f;
    float corners[4][2] = {{-sz, -sz}, {sz, -sz}, {sz, sz}, {-sz, sz}};
    for (int i = 0; i < 4; ++i) {
      Point2D p1 =
          Project3D(rot(corners[i][0], corners[i][1], 20), center, fScale);
      Point2D p2 =
          Project3D(rot(corners[i][0], corners[i][1], ch), center, fScale);
      if (p1.visible && p2.visible)
        draw->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y),
                      IM_COL32(120, 120, 130, 255), 3.0f);
    }
    float lift = Model().GetAxisPos(3) * 1.2f;
    Point2D tp1 =
        Project3D(rot(-sz * 0.9f, -sz * 0.9f, 25 + lift), center, fScale);
    Point2D tp2 =
        Project3D(rot(sz * 0.9f, sz * 0.9f, 35 + lift), center, fScale);
    if (tp1.visible && tp2.visible)
      draw->AddRectFilled(ImVec2(tp1.x, tp1.y), ImVec2(tp2.x, tp2.y),
                          IM_COL32(0, 180, 255, 200));

    // Cargo Lock (Axis 4) - Rotating Arm on top of Lift
    float lock_angle = Model().GetAxisPos(4) * 0.0174f; // Deg to Rad
    float lx = cos(lock_angle) * sz * 0.8f;
    float ly = sin(lock_angle) * sz * 0.8f;
    Point2D l1 = Project3D(rot(-lx, -ly, 38 + lift), center, fScale);
    Point2D l2 = Project3D(rot(lx, ly, 38 + lift), center, fScale);
    if (l1.visible && l2.visible)
      draw->AddLine(ImVec2(l1.x, l1.y), ImVec2(l2.x, l2.y),
                    IM_COL32(255, 100, 0, 255), 5.0f);
  }

  // Lidar
  auto scan = Model().GetHardware()->GetLidar()->GetScanData();
  for (int i = 0; i < (int)scan.ranges.size(); ++i) {
    if (scan.ranges[i] < scan.range_max) {
      float a = odom.theta + i * scan.angle_increment;
      Point2D lp = Project3D(
          {rx + scan.ranges[i] * cos(a), ry + scan.ranges[i] * sin(a), 2.0f},
          center, fScale);
      if (lp.visible) {
        draw->AddCircleFilled(ImVec2(lp.x, lp.y), 3.0f * lp.scale,
                              IM_COL32(255, 0, 0, 60));
        draw->AddCircleFilled(ImVec2(lp.x, lp.y), 1.2f * lp.scale,
                              IM_COL32(255, 50, 50, 200));
      }
    }
  }

  // HUD Overlay
  ImGui::SetCursorPos(ImVec2(10, 10));
  ImGui::BeginChild("HUD", ImVec2(220, 220), true,
                    ImGuiWindowFlags_NoBackground |
                        ImGuiWindowFlags_NoDecoration);
  ImGui::Checkbox("Follow Camera", &m_cam_follow);
  static int current_type = (int)agv_type;
  const char *types[] = {"Basic AGV", "Forker", "CTU"};
  if (ImGui::Combo("AGV Model", &current_type, types, 3)) {
    Model().SetAgvType((amr::AgvType)current_type);
  }
  ImGui::Separator();
  ImGui::TextColored(ImVec4(0, 1, 1, 1), "POSE:");
  ImGui::Text("X: %.1f m", odom.x / 100.0f);
  ImGui::Text("Y: %.1f m", odom.y / 100.0f);
  ImGui::Text("Theta: %.1f deg", (float)odom.theta * 57.29f);
  auto twist = Model().GetTwist();
  ImGui::TextColored(ImVec4(1, 1, 0, 1), "VELOCITY:");
  ImGui::Text("VX: %.2f  VY: %.2f", twist.linear_x, twist.linear_y);
  ImGui::Text("W:  %.2f", twist.angular_z);
  ImGui::EndChild();

  ImGui::EndChild();
}

void SimulatorView::Draw2DCanvas() {
  ImDrawList *draw = ImGui::GetWindowDrawList();
  ImVec2 p_min = ImGui::GetWindowPos();
  ImVec2 p_max = ImVec2(p_min.x + ImGui::GetWindowWidth(),
                        p_min.y + ImGui::GetWindowHeight());
  draw->AddRectFilled(p_min, p_max, IM_COL32(40, 40, 40, 255));
  for (const auto &p : amr::SimulatorCore::Instance().GetParticles()) {
    draw->AddCircleFilled(ImVec2(p_min.x + p.x, p_min.y + p.y), 3.0f, p.color);
  }
  for (const auto &cmd : amr::SimulatorCore::Instance().GetDrawQueue()) {
    ImVec2 p{p_min.x + cmd.x, p_min.y + cmd.y};
    if (cmd.type == amr::CmdType::RECT)
      draw->AddRectFilled(p, ImVec2(p.x + cmd.w, p.y + cmd.h), cmd.color);
    else if (cmd.type == amr::CmdType::CIRCLE)
      draw->AddCircleFilled(p, cmd.r, cmd.color);
    else if (cmd.type == amr::CmdType::TEXT)
      draw->AddText(p, cmd.color, cmd.text.c_str());
  }
}

} // namespace gui
