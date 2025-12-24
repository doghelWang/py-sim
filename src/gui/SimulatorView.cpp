#include "gui/SimulatorView.hpp"
#include "amr/AppModel.hpp"
#include "amr/ServiceContext.hpp"
#include "amr/PhysicsContext.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <vector>

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
  float cp_x = m_cam.x + m_cam.dist * cos(m_cam.pitch) * cos(m_cam.yaw);
  float cp_y = m_cam.y + m_cam.dist * cos(m_cam.pitch) * sin(m_cam.yaw);
  float cp_z = m_cam.z + m_cam.dist * sin(-m_cam.pitch);

  float dx = p.x - cp_x;
  float dy = p.y - cp_y;
  float dz = p.z - cp_z;

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
  auto physics = amr::ServiceContext::Instance().Get<amr::PhysicsContext>();
  if (!physics) return;

  ImGui::BeginChild("View3D", ImVec2(0, 0), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);
  ImDrawList *draw = ImGui::GetWindowDrawList();
  ImVec2 p_min = ImGui::GetWindowPos();
  ImVec2 p_max = ImVec2(p_min.x + ImGui::GetWindowWidth(),
                        p_min.y + ImGui::GetWindowHeight());


  // Lighting Helper
  auto LightColor = [](ImU32 col, const Point3D& n) -> ImU32 {
      // Light Dir (Top-Right-Front)
      float lx = 0.5f, ly = -0.5f, lz = 0.7f; // Normalized roughly
      float len = sqrt(lx*lx + ly*ly + lz*lz);
      lx/=len; ly/=len; lz/=len;
      
      float dot = n.x*lx + n.y*ly + n.z*lz;
      float intensity = 0.4f + 0.6f * std::max(0.0f, dot); // Ambient 0.4

      int r = (col) & 0xFF;
      int g = (col >> 8) & 0xFF;
      int b = (col >> 16) & 0xFF;
      int a = (col >> 24) & 0xFF;

      r = (int)(r * intensity);
      g = (int)(g * intensity);
      b = (int)(b * intensity);
      
      return (a << 24) | (b << 16) | (g << 8) | r;
  };

  if (ImGui::IsWindowHovered()) {
    ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
      m_cam.yaw -= io.MouseDelta.x * 0.01f;
      m_cam.pitch -= io.MouseDelta.y * 0.01f;
      m_cam.pitch = std::clamp(m_cam.pitch, -1.5f, -0.1f);
      m_cam_follow = false;
    }
    // Logarithmic Zoom
    if (io.MouseWheel != 0.0f) {
        float zoom_factor = 1.0f - io.MouseWheel * 0.1f;
        m_cam.dist *= zoom_factor;
        m_cam.dist = std::clamp(m_cam.dist, 20.0f, 5000.0f);
    }
  }

  ImVec2 center =
      ImVec2((p_min.x + p_max.x) * 0.5f, (p_min.y + p_max.y) * 0.5f);
  draw->AddRectFilled(p_min, p_max, IM_COL32(30, 30, 30, 255));

  float dt = ImGui::GetIO().DeltaTime;
  auto odom = physics->GetOdometry();

  if (m_cam_follow) {
    float l = 1.0f - std::exp(-5.0f * dt);
    m_cam.x += ((float)odom.x - m_cam.x) * l;
    m_cam.y += ((float)odom.y - m_cam.y) * l;
    m_cam.z = 20.0f; 
  }

  float fScale = 500.0f;
  // Floor Grid - Simplified to be subtle
  auto draw_grid = [&](float step, ImU32 col) {
    float sx = std::floor((m_cam.x - 3000) / step) * step;
    float ex = std::floor((m_cam.x + 3000) / step) * step;
    float sy = std::floor((m_cam.y - 3000) / step) * step;
    float ey = std::floor((m_cam.y + 3000) / step) * step;
    for (float x = sx; x <= ex; x += step) {
      Point2D p1 = Project3D({x, sy, 0}, center, fScale),
              p2 = Project3D({x, ey, 0}, center, fScale);
      if (p1.visible && p2.visible)
        draw->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), col, 1.0f);
    }
    for (float y = sy; y <= ey; y += step) {
      Point2D p1 = Project3D({sx, y, 0}, center, fScale),
              p2 = Project3D({ex, y, 0}, center, fScale);
      if (p1.visible && p2.visible)
        draw->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), col, 1.0f);
    }
  };
  draw_grid(100.0f, IM_COL32(60, 60, 65, 255)); // Faint line

  // Obstacles (Lit)
  for (const auto &w : physics->GetObstacles()) {
    float wx = w.x, wy = w.y, ww = w.w, wh = w.h, wz = 50.0f;
    // Normals: Top(0,0,1), Front(0,1,0), Right(1,0,0) etc.
    // For sim, just fixed coloring based on face type
    
    // Top Face
    Point2D t[4] = {Project3D({wx, wy, wz}, center, fScale),
                    Project3D({wx + ww, wy, wz}, center, fScale),
                    Project3D({wx + ww, wy + wh, wz}, center, fScale),
                    Project3D({wx, wy + wh, wz}, center, fScale)};
    
    if (t[0].visible) {
       ImU32 top_col = LightColor(IM_COL32(120, 120, 140, 255), {0,0,1});
       draw->AddQuadFilled(ImVec2(t[0].x, t[0].y), ImVec2(t[1].x, t[1].y),
                           ImVec2(t[2].x, t[2].y), ImVec2(t[3].x, t[3].y), top_col);
       draw->AddQuad(ImVec2(t[0].x, t[0].y), ImVec2(t[1].x, t[1].y), // Outline
                           ImVec2(t[2].x, t[2].y), ImVec2(t[3].x, t[3].y), IM_COL32(200,200,200,100));
    }
    // Side faces would need sorting, skipping for simple "Top-Down-ish" view optimization
  }

  // AGV
  float rx = (float)odom.x, ry = (float)odom.y, sz = 25.0f;
  float ct = cos(odom.theta), st = sin(odom.theta);
  auto rot = [&](float x, float y, float z = 0) {
    return Point3D{rx + x * ct - y * st, ry + x * st + y * ct, z};
  };

  auto agv_type = Model().GetAgvType();
  ImU32 base_col = IM_COL32(200, 200, 210, 255); // White-ish
  if(agv_type == amr::AgvType::BASIC) base_col = IM_COL32(100, 100, 200, 255); // Blue
  
  float chassis_h = 20.0f;
  if(agv_type == amr::AgvType::FORKER) chassis_h = 12.0f;
  
  // Body (Lit)
  Point2D rt[4] = {Project3D(rot(-sz, -sz, chassis_h), center, fScale),
                   Project3D(rot(sz, -sz, chassis_h), center, fScale),
                   Project3D(rot(sz, sz, chassis_h), center, fScale),
                   Project3D(rot(-sz, sz, chassis_h), center, fScale)};

  if (rt[0].visible) {
     ImU32 top_col = LightColor(base_col, {0,0,1});
     draw->AddQuadFilled(ImVec2(rt[0].x, rt[0].y), ImVec2(rt[1].x, rt[1].y),
                         ImVec2(rt[2].x, rt[2].y), ImVec2(rt[3].x, rt[3].y),
                         top_col);
     // Direction Indicator (Triangle on top)
     Point2D dt1 = Project3D(rot(sz*0.6f, 0, chassis_h+1), center, fScale);
     Point2D dt2 = Project3D(rot(-sz*0.4f, -sz*0.4f, chassis_h+1), center, fScale);
     Point2D dt3 = Project3D(rot(-sz*0.4f, sz*0.4f, chassis_h+1), center, fScale);
     draw->AddTriangleFilled(ImVec2(dt1.x, dt1.y), ImVec2(dt2.x, dt2.y), ImVec2(dt3.x, dt3.y), IM_COL32(30,30,30,150));
  }

  // Draw Wheels
  ImU32 wheel_col = IM_COL32(40, 40, 40, 255);
  float w_off_x = sz * 0.8f;
  float w_off_y = sz * 1.0f + 4.0f; // Outside chassis
  float w_rad = 8.0f;
  float w_width = 4.0f;
  
  // 4 Wheels
  float wheel_pos[4][2] = {{-w_off_x, -w_off_y}, {w_off_x, -w_off_y}, {w_off_x, w_off_y}, {-w_off_x, w_off_y}};
  for(int i=0; i<4; ++i) {
      Point3D wc = rot(wheel_pos[i][0], wheel_pos[i][1], w_rad);
      // Simple 3D Box for wheel
      Point2D p1 = Project3D({wc.x - w_width, wc.y - w_rad, 0}, center, fScale);
      Point2D p2 = Project3D({wc.x + w_width, wc.y - w_rad, 0}, center, fScale);
      Point2D p3 = Project3D({wc.x + w_width, wc.y + w_rad, 2*w_rad}, center, fScale);
      Point2D p4 = Project3D({wc.x - w_width, wc.y + w_rad, 2*w_rad}, center, fScale);
      // Just drawing a filled circle/blob for simplicity at distance
      Point2D p_center = Project3D(wc, center, fScale);
      if(p_center.visible) {
          draw->AddCircleFilled(ImVec2(p_center.x, p_center.y), w_rad * p_center.scale, wheel_col);
          // Rim
          draw->AddCircleFilled(ImVec2(p_center.x, p_center.y), w_rad * 0.6f * p_center.scale, IM_COL32(100,100,100,255));
      }
  }

  // IO Feedback
  for (int i = 0; i < 4; ++i) {
    if (Model().GetDO(i)) {
      Point2D p = Project3D(rot(-sz - 2, -10 + i * 10, chassis_h - 5), center, fScale);
      if (p.visible)
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f * p.scale, IM_COL32(255, 180, 0, 255));
    }
    if (Model().GetDI(i)) {
      Point2D p = Project3D(rot(sz + 2, -10 + i * 10, 5), center, fScale);
      if (p.visible) {
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f * p.scale, IM_COL32(0, 255, 0, 255));
        Point2D p_end = Project3D(rot(sz + 40, -10 + i * 10, 5), center, fScale);
        if (p_end.visible)
          draw->AddLine(ImVec2(p.x, p.y), ImVec2(p_end.x, p_end.y), IM_COL32(0, 255, 0, 100), 2.0f);
      }
    }
  }

  Point2D h = Project3D(rot(sz * 1.2f, 0, chassis_h + 1), center, fScale),
          l = Project3D(rot(sz * 0.5f, -sz * 0.5f, chassis_h + 1), center, fScale),
          r = Project3D(rot(sz * 0.5f, sz * 0.5f, chassis_h + 1), center, fScale);
  if (h.visible)
    draw->AddTriangleFilled(ImVec2(h.x, h.y), ImVec2(l.x, l.y), ImVec2(r.x, r.y), IM_COL32(255, 255, 255, 200));

  if (agv_type == amr::AgvType::FORKER) {
    Point3D m_tl = rot(sz - 5, -sz * 0.8f, 120), m_tr = rot(sz - 5, sz * 0.8f, 120);
    Point2D p_tl = Project3D(m_tl, center, fScale), p_tr = Project3D(m_tr, center, fScale);
    Point2D p_bl = Project3D(rot(sz - 5, -sz * 0.8f, 20), center, fScale);
    Point2D p_br = Project3D(rot(sz - 5, sz * 0.8f, 20), center, fScale);
    if (p_tl.visible && p_tr.visible) {
      draw->AddLine(ImVec2(p_bl.x, p_bl.y), ImVec2(p_tl.x, p_tl.y), IM_COL32(80, 80, 85, 255), 6.0f);
      draw->AddLine(ImVec2(p_br.x, p_br.y), ImVec2(p_tr.x, p_tr.y), IM_COL32(80, 80, 85, 255), 6.0f);
      draw->AddLine(ImVec2(p_tl.x, p_tl.y), ImVec2(p_tr.x, p_tr.y), IM_COL32(100, 100, 110, 255), 4.0f);
    }
    float lift = Model().GetAxisPos(3) * 0.8f;
    for (float s : {-sz * 0.5f, sz * 0.5f}) {
      Point2D fp1 = Project3D(rot(sz + 5, s, 30 + lift), center, fScale);
      Point2D fp2 = Project3D(rot(sz + 55, s, 28 + lift), center, fScale);
      if (fp1.visible && fp2.visible)
        draw->AddLine(ImVec2(fp1.x, fp1.y), ImVec2(fp2.x, fp2.y), IM_COL32(40, 40, 40, 255), 8.0f * fp1.scale);
    }
  } else if (agv_type == amr::AgvType::CTU) {
    float ch = 180.0f;
    float corners[4][2] = {{-sz, -sz}, {sz, -sz}, {sz, sz}, {-sz, sz}};
    for (int i = 0; i < 4; ++i) {
      Point2D p1 = Project3D(rot(corners[i][0], corners[i][1], 20), center, fScale);
      Point2D p2 = Project3D(rot(corners[i][0], corners[i][1], ch), center, fScale);
      if (p1.visible && p2.visible)
        draw->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), IM_COL32(120, 120, 130, 255), 3.0f);
    }
    float lift = Model().GetAxisPos(3) * 1.2f;
    Point2D tp1 = Project3D(rot(-sz * 0.9f, -sz * 0.9f, 25 + lift), center, fScale);
    Point2D tp2 = Project3D(rot(sz * 0.9f, sz * 0.9f, 35 + lift), center, fScale);
    if (tp1.visible && tp2.visible)
      draw->AddRectFilled(ImVec2(tp1.x, tp1.y), ImVec2(tp2.x, tp2.y), IM_COL32(0, 180, 255, 200));

    float lock_angle = Model().GetAxisPos(4) * 0.0174f; 
    float lx = cos(lock_angle) * sz * 0.8f;
    float ly = sin(lock_angle) * sz * 0.8f;
    Point2D l1 = Project3D(rot(-lx, -ly, 38 + lift), center, fScale);
    Point2D l2 = Project3D(rot(lx, ly, 38 + lift), center, fScale);
    if (l1.visible && l2.visible)
      draw->AddLine(ImVec2(l1.x, l1.y), ImVec2(l2.x, l2.y), IM_COL32(255, 100, 0, 255), 5.0f);
  }

  // Lidar
  auto hw = Model().GetHardware();
  if (hw) {
      auto scan = hw->GetLidar()->GetScanData();
      for (size_t i = 0; i < scan.ranges.size(); ++i) {
        if (scan.ranges[i] < scan.range_max) {
          float a = odom.theta + i * scan.angle_increment;
          Point2D lp = Project3D(
              {rx + scan.ranges[i] * cos(a), ry + scan.ranges[i] * sin(a), 2.0f},
              center, fScale);
          if (lp.visible) {
            draw->AddCircleFilled(ImVec2(lp.x, lp.y), 3.0f * lp.scale, IM_COL32(255, 0, 0, 60));
            draw->AddCircleFilled(ImVec2(lp.x, lp.y), 1.2f * lp.scale, IM_COL32(255, 50, 50, 200));
          }
        }
      }
  }

  ImGui::EndChild();
}

// Local storage for draw commands (persisted between frames until cleared)
static std::vector<amr::DrawCmd> g_draw_cmds;

void SimulatorView::Draw2DCanvas() {
  auto physics = amr::ServiceContext::Instance().Get<amr::PhysicsContext>();
  if (!physics) return;

  // Sync Draw Commands
  auto new_cmds = Model().SwapDrawQueue();
  for(const auto& cmd : new_cmds) {
      if(cmd.type == amr::CmdType::CLEAR) {
          g_draw_cmds.clear();
      } else {
          g_draw_cmds.push_back(cmd);
      }
  }

  ImDrawList *draw = ImGui::GetWindowDrawList();
  ImVec2 p_min = ImGui::GetWindowPos();
  ImVec2 p_max = ImVec2(p_min.x + ImGui::GetWindowWidth(),
                        p_min.y + ImGui::GetWindowHeight());
  draw->AddRectFilled(p_min, p_max, IM_COL32(40, 40, 40, 255));
  
  for (const auto &p : physics->GetParticles()) {
    draw->AddCircleFilled(ImVec2(p_min.x + p.x, p_min.y + p.y), 3.0f, p.color);
  }

  for (const auto &cmd : g_draw_cmds) {
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
