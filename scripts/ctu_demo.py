# model: CTU
import host_api
import time

def main():
    host_api.log_message("--- CTU Demo Started ---")
    host_api.log_message("[1] Moving to rack A-04...")
    # Updated to scaled units: 0.4 m/s (was 40.0 cm/s)
    host_api.set_twist(0.4, 0, 0)
    host_api.sleep_ms(4000)
    host_api.set_twist(0, 0, 0)
    
    host_api.log_message("[2] Positioning Mast (Simulating Alignment)...")
    host_api.sleep_ms(1000)
        
    host_api.log_message("[3] Aligned. Elevating for bin retrieval (Shelf 3)...")
    host_api.axis_move(3, 120.0, 20.0) # Axis 3 is CTU elevator mechanism (Units: CM)
    while host_api.axis_is_moving(3):
        host_api.sleep_ms(50)
        if host_api.should_terminate(): return
        
    host_api.sleep_ms(1000)

    host_api.log_message("[4] Bin retrieved. Returning to home position...")
    host_api.set_twist(-0.4, 0, 0)
    host_api.sleep_ms(4000)
    host_api.set_twist(0, 0, 0)
    host_api.axis_move(3, 0.0, 20.0) # Retract
    while host_api.axis_is_moving(3):
        host_api.sleep_ms(50)

    host_api.log_message("--- CTU Demo Finished ---")

if __name__ == "__main__":
    main()
