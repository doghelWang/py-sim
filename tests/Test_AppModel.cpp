#include "amr/AppModel.hpp"
#include <gtest/gtest.h>

// Helper to reset app state (since it's a singleton)
void ResetApp(amr::AppModel &app) {
  app.SetPaused(false);
  for (int i = 0; i < 3; ++i) {
    app.AxisMove(i, 0.0f, 0.0f);
    app.SetAxisCurrentPos(i, 0.0f);
  }
}

// Test Singleton Access
TEST(AppModelTest, SingletonAccess) {
  amr::AppModel &app = amr::AppModel::Instance();
  EXPECT_FALSE(app.IsRunning());
}

// Test Axis Logic
TEST(AppModelTest, AxisMovement) {
  amr::AppModel &app = amr::AppModel::Instance();
  ResetApp(app);

  // Initial state
  EXPECT_FLOAT_EQ(app.GetAxisPos(0), 0.0f);
  EXPECT_FALSE(app.IsAxisMoving(0));

  // Move
  app.AxisMove(0, 100.0f, 10.0f);
  EXPECT_TRUE(app.IsAxisMoving(0));

  // Physics Update (1 sec at 10 u/s -> +10)
  app.UpdateSafetyLogic(1.0f);
  EXPECT_FALSE(app.IsPaused()); // Ensure not paused

  // Position should be 10.0
  EXPECT_NEAR(app.GetAxisPos(0), 10.0f, 0.001f);
  EXPECT_TRUE(app.IsAxisMoving(0));

  // Physics Update (9 more secs -> 90 + 10 = 100)
  for (int i = 0; i < 9; ++i)
    app.UpdateSafetyLogic(1.0f);

  EXPECT_NEAR(app.GetAxisPos(0), 100.0f, 0.001f);

  // Should be stopped
  app.UpdateSafetyLogic(0.1f); // Final check
  EXPECT_FALSE(app.IsAxisMoving(0));
}

// Test Pause Logic
TEST(AppModelTest, PausePreventsMotion) {
  amr::AppModel &app = amr::AppModel::Instance();
  ResetApp(app);

  app.AxisMove(0, 100.0f, 10.0f);
  app.SetPaused(true);

  EXPECT_TRUE(app.IsPaused());

  // Physics should NOT update
  app.UpdateSafetyLogic(1.0f);
  EXPECT_FLOAT_EQ(app.GetAxisPos(0), 0.0f);

  app.SetPaused(false);
  app.UpdateSafetyLogic(1.0f);
  EXPECT_NEAR(app.GetAxisPos(0), 10.0f, 0.001f);
}

// Test I/O
TEST(AppModelTest, DigitalIO) {
  amr::AppModel &app = amr::AppModel::Instance();
  ResetApp(app);

  app.SetDO(2, true);
  EXPECT_TRUE(app.GetDO(2));
  EXPECT_FALSE(app.GetDO(1));

  app.SetDI(5, true); // Simulator setter
  EXPECT_TRUE(app.GetDI(5));
}

// Test Safety Logic (New)
TEST(AppModelTest, SafetyStop) {
  amr::AppModel &app = amr::AppModel::Instance();
  ResetApp(app);

  // Map DI-6 to ESTOP
  app.MapInput(6, amr::InputAction::ESTOP, false, false);

  // Simulate High Signal
  app.SetDI(6, true);
  app.UpdateSafetyLogic(0.016f);

  EXPECT_TRUE(app.IsPaused()); // Should be paused

  // Try to unpause logic (e.g. via GUI button simulation)
  app.SetPaused(false);
  // Update safety logic again (it runs every frame)
  app.UpdateSafetyLogic(0.016f);
  EXPECT_TRUE(app.IsPaused()); // Should force pause back

  // Release Signal
  app.SetDI(6, false);
  app.UpdateSafetyLogic(0.016f);
  // App should remain paused until manually resumed
  EXPECT_TRUE(app.IsPaused());

  // Manual Resume
  app.SetPaused(false);
  EXPECT_FALSE(app.IsPaused());
}

// Test Safety Config & E-Stop Logic
TEST(AppModelTest, SafetyConfigTest) {
  amr::AppModel &app = amr::AppModel::Instance();
  ResetApp(app);

  // Map DI-6 to ESTOP (High Active, Level Trigger)
  app.MapInput(6, amr::InputAction::ESTOP, false, false);

  // Start Axis Moving
  app.AxisMove(0, 100.0f, 10.0f);
  app.UpdateSafetyLogic(0.1f); // Move a bit
  EXPECT_TRUE(app.IsAxisMoving(0));
  EXPECT_FALSE(app.IsPaused());

  // Trigger DI-6 (E-Stop)
  app.SetDI(6, true);
  app.UpdateSafetyLogic(0.016f); // Process Safety Logic

  // Should be Paused immediately
  EXPECT_TRUE(app.IsPaused());

  // Motion should stop (Motion Freeze due to Pause)
  float pos_before = app.GetAxisPos(0);
  app.UpdateSafetyLogic(1.0f);
  float pos_after = app.GetAxisPos(0);
  EXPECT_FLOAT_EQ(pos_before, pos_after);

  // Release DI-6 (E-Stop Released)
  app.SetDI(6, false);
  app.UpdateSafetyLogic(0.016f);

  // System should STAY Paused (Safety requirement: Manual Resume)
  EXPECT_TRUE(app.IsPaused());

  // Manual Resume
  app.SetPaused(false);
  EXPECT_FALSE(app.IsPaused());

  // Motion Resumes
  app.UpdateSafetyLogic(1.0f);
  EXPECT_GT(app.GetAxisPos(0), pos_after);
}
