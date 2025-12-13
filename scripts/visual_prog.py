import time
import threading
import sys
import os
sys.path.append(os.getcwd() + '/../src')
try:
    import host_api
except ImportError:
    # Mock for testing
    class HostApi:
        def log_message(self, msg): print(msg)
        def axis_move(self, a, p, v): print(f'Move {a} {p} {v}')
        def axis_is_moving(self, a): return False
        def sleep_ms(self, ms): time.sleep(ms/1000.0)
        def set_do(self, p, v): print(f'DO {p} {v}')
        def get_di(self, p): return False
        def set_reg(self, r, v): pass
        def get_reg(self, r): return 0.0
        def get_param(self, n): return 0.0
        def set_paused(self, p): pass
    host_api = HostApi()

def init():
    host_api.log_message('[Sys] Initializing Safety Config...')
    # Clear previous safety (Important if script is re-run logic, though AppModel has ResetSafetyConfig)
    host_api.configure_input(6, 1, False, False)
    host_api.configure_input(7, 3, False, False)
    host_api.log_message('[Sys] Safety Configured.')

def main():
    host_api.log_message('Starting AMR Logic...')
    if host_api.get_param('angle') <= 0:
        host_api.log_message('Twarn: Vel=0 Check angle')
    host_api.axis_move(1, 90.00, host_api.get_param('angle'))
    while host_api.axis_is_moving(1):
        host_api.sleep_ms(10)
    host_api.set_reg(1, 1.00)
    if host_api.get_param('height') <= 0:
        host_api.log_message('Twarn: Vel=0 Check height')
    host_api.axis_move(0, 100.00, host_api.get_param('height'))
    while host_api.axis_is_moving(0):
        host_api.sleep_ms(10)
    if host_api.get_reg(1) == 1.00:
        host_api.set_reg(2, 1.00)
        host_api.axis_move(1, 0.00, 2.00)
        while host_api.axis_is_moving(1):
            host_api.sleep_ms(10)
        if host_api.get_reg(2) == 1.00:
            host_api.set_reg(5, 1.00)
            host_api.axis_move(0, 0.00, 2.00)
            while host_api.axis_is_moving(0):
                host_api.sleep_ms(10)
    host_api.log_message('Program Complete.')


if __name__ == '__main__':
    init()
    main()
