import host_api
import time

def some_business_logic():
    print("PYTHON: Starting business logic...")
    host_api.log_message("Hello from Python! Initialization complete.")
    
    total = 0
    # Loop for a long time to allow pause/resume testing
    for i in range(1, 1000):
        # Simulate work
        time.sleep(1.0) 
        total += i
        
        # Call host API
        host_api.log_message(f"PYTHON: Processing step {i}, current total={total}")
        
        # Variable for inspection
        status = "WORKING"
        
    print("PYTHON: Logic finished.")
    return total

if __name__ == "__main__":
    some_business_logic()
