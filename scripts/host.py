
import sys
try:
    import host_api
except ImportError:
    # Fallback for testing outside embedded env
    class MockHost:
        def log(self, msg): print(f"[MOCK] {msg}")
        def move_axis_abs(self, a, p, v): print(f"[MOCK] Move Axis {a} to {p}")
        def sleep_ms(self, ms): time.sleep(ms/1000.0)
        def __getattr__(self, name): return lambda *args: None
    host_api = MockHost()

# Copy everything from host_api to this module
for name in dir(host_api):
    if not name.startswith("__"):
        globals()[name] = getattr(host_api, name)

# Alias specific functions if names differ
if hasattr(host_api, 'axis_move'):
    move_axis_abs = host_api.axis_move
if hasattr(host_api, 'log_message'):
    log = host_api.log_message
