import requests
import json
import time
import threading
import sys
import datetime

BASE_URL = "http://localhost:8080"
LOG_FILE = "tests/traffic_log.txt"

def log(section, direction, content):
    ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
    msg = f"[{ts}] [{section}] {direction}:\n{content}\n" + "-"*40 + "\n"
    print(msg)
    with open(LOG_FILE, "a", encoding='utf-8') as f:
        f.write(msg)

def listen_sse():
    try:
        with requests.get(f"{BASE_URL}/api/stream", stream=True, timeout=10) as r:
            log("SSE", "REQ", f"GET /api/stream")
            for line in r.iter_lines():
                if line:
                    decoded = line.decode('utf-8')
                    log("SSE", "RX", decoded)
    except Exception as e:
        pass

def run_tests():
    # 0. Clear Log
    with open(LOG_FILE, "w") as f: f.write("Traffic Log\n================\n")

    # 1. Config
    log("API:Config", "TX", "GET /api/config")
    r = requests.get(f"{BASE_URL}/api/config")
    log("API:Config", "RX", f"Status: {r.status_code}\nBody: {json.dumps(r.json(), indent=2)}")
    
    # 2. Schema
    log("API:Schema", "TX", "GET /api/schema")
    r = requests.get(f"{BASE_URL}/api/schema")
    log("API:Schema", "RX", f"Status: {r.status_code}\nBody: {json.dumps(r.json(), indent=2)}")

    # 3. Trigger Run
    payload = {
        "type": "code",
        "content": "import host\nimport time\nhost.print('Test Start')\nhost.move_axis_abs(0, 50, 10)\ntime.sleep(0.5)\nhost.print('Test End')"
    }
    log("API:Run", "TX", f"POST /api/run\nBody: {json.dumps(payload, indent=2)}")
    r = requests.post(f"{BASE_URL}/api/run", json=payload)
    log("API:Run", "RX", f"Status: {r.status_code}\nBody: {r.text}")

    time.sleep(2)
    
    # 4. Stop
    log("API:Stop", "TX", "POST /api/stop")
    r = requests.post(f"{BASE_URL}/api/stop")
    log("API:Stop", "RX", f"Status: {r.status_code}\nBody: {r.text}")

if __name__ == "__main__":
    t = threading.Thread(target=listen_sse, daemon=True)
    t.start()
    time.sleep(1) 
    run_tests()
    time.sleep(1) # Drain SSE
