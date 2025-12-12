import host_api
import time

def move_wait(axis, pos, vel):
    host_api.log_message(f"[PLC] Axis {axis} -> {pos}")
    host_api.axis_move(axis, pos, vel)
    while host_api.axis_is_moving(axis):
        host_api.sleep_ms(10)
    host_api.log_message(f"[PLC] Axis {axis} Done.")

def drill():
    host_api.log_message("[PLC] Drilling...")
    move_wait(2, 50.0, 5.0) # Down
    host_api.sleep_ms(200)
    move_wait(2, 0.0, 10.0) # Up

def main():
    host_api.log_message("Starting Machine Motion Profile...")
    host_api.log_message("Shape: Square 200x150")
    
    # Home
    host_api.axis_move(0, 0, 20)
    host_api.axis_move(1, 0, 20)
    host_api.axis_move(2, 0, 20)
    while host_api.axis_is_moving(0) or host_api.axis_is_moving(1):
        host_api.sleep_ms(10)
        
    speed = 4.0
    
    # Corner 1 (0,0)
    drill()
    
    # Corner 2 (200, 0)
    move_wait(0, 200.0, speed)
    drill()
    
    # Corner 3 (200, 150)
    move_wait(1, 150.0, speed)
    drill()
    
    # Corner 4 (0, 150)
    move_wait(0, 0.0, speed)
    drill()
    
    # Home (0, 0)
    move_wait(1, 0.0, speed)
    
    host_api.log_message("[PLC] Cycle Complete.")

if __name__ == "__main__":
    main()
