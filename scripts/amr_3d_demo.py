
import host_api
import time
import math

def run():
    host_api.log_message("Starting High-Fidelity AMR 3D Demo...")
    
    # 1. Reset
    # host_api.estop_reset() 
    # Not exposed directly, usually cleared via UI or digital input simulation
    
    # 2. Config Axis (Simulated)
    # Set to Velocity Mode for continuous driving
    # Axis 0: X, Axis 1: Y, Axis 2: Theta (for Mecanum / Omni)
    # Using legacy axis_move for now which maps to Position/Velocity in SimHardware
    
    # Let's clean slate
    host_api.axis_move(0, 0, 100) # Reset Pos
    host_api.axis_move(1, 0, 100)
    host_api.axis_move(2, 0, 100)
    time.sleep(1)
    
    t = 0
    dt = 0.1
    
    host_api.log_message("Driving in a circle...")
    
    while t < 20.0:
        if host_api.should_terminate():
            break
            
        # Parametric Circle path
        # x = R * cos(t)
        # y = R * sin(t)
        # We can drive by updating target position continuously
        
        R = 200.0 # radius pixels
        x = R * math.cos(t * 0.5)
        y = R * math.sin(t * 0.5)
        theta = t * 0.5 + math.pi / 2 # Face tangent
        
        # In SimHardware (Position Mode), it moves towards target at max vel
        # We set target to be slightly ahead
        
        host_api.axis_move(0, x, 200)
        host_api.axis_move(1, y, 200)
        host_api.axis_move(2, theta, 2.0)
        
        # Audio feedback on some interval
        if int(t*10) % 20 == 0:
             # host_api.play_audio(...) # If available
             pass
             
        # Visual Markers (Legacy DrawQueue still works on top)
        if int(t*10) % 5 == 0:
             host_api.draw_circle(x, y, 5, 0, 255, 0) # Green trail
        
        time.sleep(dt)
        t += dt

    host_api.log_message("Demo Completed.")

if __name__ == "__main__":
    run()
run()
