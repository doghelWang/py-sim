import host
import time

# CTU-600 Demo Script
# Using native API commands that match the Schema

host.log("Starting CTU Demo Sequence...")

# 1. Lift Up
host.log("Lifting...")
host.move_axis_abs(3, 500.0, 100.0) # Axis 3 = Lift
time.sleep(1.0)

# 2. Move Forward
host.log("Moving Forward...")
host.move_axis_abs(0, 2000.0, 500.0) # Axis 0 = X
time.sleep(2.0)

# 3. Rotate
host.log("Rotating...")
host.move_axis_abs(2, 1.57, 1.0) # Axis 2 = Theta (90 deg)
time.sleep(1.0)

# 4. Move Sideways (in local frame of robot after rotation?)
# Note: move_axis_abs moves physical axes. 
# If robot rotated, X axis is still robot's longitudinal axis.
host.log("Moving Forward (New Heading)...")
host.move_axis_abs(0, 3000.0, 500.0)
time.sleep(2.0)

host.log("Demo Complete")
