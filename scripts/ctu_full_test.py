import time
import sys
import os

# Ensure src path is visible so we can mock HostApi if needed
sys.path.append(os.getcwd() + '/../src')

# model: CTU

try:
    import host_api
except ImportError:
    class HostApiPlaceholder:
        def log_message(self, msg): print(msg)
        def axis_move(self, a, p, v): print(f'Move {a} {p} {v}')
        def axis_is_moving(self, a): return False
        def sleep_ms(self, ms): time.sleep(ms/1000.0)
        def set_twist(self, vx, vy, wz): print(f'Twist {vx} {vy} {wz}')
        def configure_input(self, p, a, i, e): pass
    host_api = HostApiPlaceholder()

def init():
    host_api.log_message('[Test] CTU Motion Test Init...')
    # Configure safety to ensure no accidental E-Stops
    # Assuming Input 0 is Estop, set it carefully or clear it
    # We rely on AppModel::ClearSafety() happening on load
    pass

def main():
    host_api.log_message('--- Starting CTU Motion Test ---')
    host_api.sleep_ms(1000)

    # 1. Chassis Motion
    host_api.log_message('[1/6] Move Forward')
    host_api.set_twist(0.5, 0.0, 0.0)
    host_api.sleep_ms(2000)
    host_api.set_twist(0.0, 0.0, 0.0)
    host_api.sleep_ms(500)

    host_api.log_message('[2/6] Move Backward')
    host_api.set_twist(-0.5, 0.0, 0.0)
    host_api.sleep_ms(2000)
    host_api.set_twist(0.0, 0.0, 0.0)
    host_api.sleep_ms(500)

    host_api.log_message('[3/6] Move Left (Crab)')
    host_api.set_twist(0.0, 0.5, 0.0)
    host_api.sleep_ms(2000)
    host_api.set_twist(0.0, 0.0, 0.0)
    host_api.sleep_ms(500)

    host_api.log_message('[4/6] Move Right (Crab)')
    host_api.set_twist(0.0, -0.5, 0.0)
    host_api.sleep_ms(2000)
    host_api.set_twist(0.0, 0.0, 0.0)
    host_api.sleep_ms(500)

    # 2. Axis Motion
    # Axis 3: Elevator (Linear, 0-100 units)
    host_api.log_message('[5/6] Elevator UP')
    host_api.axis_move(3, 100.0, 30.0) # Target 100, Vel 30
    while host_api.axis_is_moving(3):
        host_api.sleep_ms(100)
    
    host_api.sleep_ms(500)
    host_api.log_message('[5/6] Elevator DOWN')
    host_api.axis_move(3, 0.0, 30.0)
    while host_api.axis_is_moving(3):
        host_api.sleep_ms(100)

    # Axis 4: Cargo Lock (Rotary, 0-90 degrees)
    host_api.log_message('[6/6] Lock Rotate 90 deg')
    host_api.axis_move(4, 90.0, 45.0) # Target 90 deg, Vel 45 deg/s
    while host_api.axis_is_moving(4):
        host_api.sleep_ms(100)
    
    host_api.sleep_ms(500)
    host_api.log_message('[6/6] Lock Rotate 0 deg')
    host_api.axis_move(4, 0.0, 45.0)
    while host_api.axis_is_moving(4):
        host_api.sleep_ms(100)

    host_api.log_message('--- Test Complete ---')

if __name__ == '__main__':
    init()
    main()
