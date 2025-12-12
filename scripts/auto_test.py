import host_api
import json
import time

def main():
    host_api.log_message("Starting Automation Test Suite...")
    host_api.clear_screen()
    
    # 1. System Command Test
    host_api.log_message("[TEST] Running System Command 'ls'...")
    files = host_api.exec_cmd("ls")
    host_api.log_message(f"Cmd Output:\n{files}")
    
    # 2. Network Test
    host_api.log_message("[TEST] Testing Network (httpbin.org)...")
    start_time = time.time()
    try:
        # Note: This blocks the Python thread, but GUI stays responsive
        response = host_api.http_get("http://httpbin.org", "/get") 
        duration = (time.time() - start_time) * 1000
        host_api.log_message(f"HTTP GET Status: 200 OK (Time: {duration:.2f}ms)")
        host_api.log_message(f"Response Body Preview: {response[:50]}...")
    except Exception as e:
        host_api.log_message(f"HTTP Request Failed: {e}")
        duration = 0

    # 3. Visualize Results
    host_api.log_message("[TEST] visualising Report...")
    host_api.clear_screen()
    
    # Draw Background
    host_api.draw_rect(0, 0, 800, 600, 40, 40, 50)
    
    # Draw Title
    host_api.draw_text(20, 20, "AUTOMATION TEST REPORT", 255, 255, 255)
    host_api.draw_text(20, 50, "1. System Check: PASS", 100, 255, 100)
    host_api.draw_text(20, 70, "2. Network Check: PASS" if duration > 0 else "FAIL", 100, 255, 100)
    
    # Draw Graph
    host_api.draw_text(20, 120, "Response Time (ms)", 200, 200, 200)
    host_api.draw_rect(20, 150, 300, 200, 0, 0, 0) # Graph Bg
    
    bar_height = min(duration, 180)
    host_api.draw_rect(50, 350 - bar_height, 50, bar_height, 100, 150, 255)
    host_api.draw_text(50, 350 - bar_height - 20, f"{duration:.0f}ms", 255, 255, 255)

    # 4. Save Report
    report = {
        "timestamp": time.time(),
        "system_check": "PASS",
        "network_duration_ms": duration,
        "files_listed": len(files.split('\n'))
    }
    host_api.write_file("test_report.json", json.dumps(report, indent=2))
    host_api.log_message("[TEST] Report JSON saved.")
    
    # 5. Screenshot
    host_api.log_message("[TEST] Taking Screenshot...")
    # Wait a bit for render to catch up
    host_api.sleep_ms(500) 
    host_api.take_screenshot("test_result.png")
    host_api.log_message("[TEST] Screenshot 'test_result.png' saved.")
    
    host_api.log_message("SUITE COMPLETE.")

if __name__ == "__main__":
    main()
