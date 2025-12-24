import time
import threading
import sys
import os
sys.path.append(os.getcwd() + '/../src')
try:
    import host_api
except ImportError:
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
    host_api.log_message('[Sys] Safety Configured.')

def main():
    host_api.log_message('Starting AMR Logic...')
    host_api.log_message('Program Complete.')

if __name__ == '__main__':
    init()
    main()
