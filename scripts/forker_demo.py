# model: FORKER
import host_api
import time

def main():
    host_api.log_message("--- Forker Demo Started ---")
    host_api.log_message("[1] Moving chassis to loading station...")
    host_api.set_twist(50.0, 20.0, 0.5)
    host_api.sleep_ms(3000)
    host_api.set_twist(0, 0, 0)
    
    host_api.log_message("[2] Waiting for Pallet Presence (DI 0)...")
    # You can trigger DI 0 manually in the Hardware View
    while not host_api.get_di(0):
        host_api.sleep_ms(100)
        if host_api.should_terminate(): return
        
    host_api.log_message("[3] Pallet Detected. Lifting forks...")
    host_api.axis_move(3, 100.0, 15.0) # Axis 3 is the forklift lift mechanism
    while host_api.axis_is_moving(3):
        host_api.sleep_ms(50)
        if host_api.should_terminate(): return
        
    host_api.log_message("[4] Loading complete. Proceeding to delivery...")
    host_api.set_twist(-30, 0, -0.2)
    host_api.sleep_ms(2000)
    host_api.set_twist(0, 0, 0)
    host_api.log_message("--- Forker Demo Finished ---")

if __name__ == "__main__":
    main()
