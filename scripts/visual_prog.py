import host_api
import time
import threading

def safety_monitor():
    di6_prev = False
    while True:
        # DI-6: Pause Logic (Edge Trigger)
        di6 = host_api.get_di(6)
        if di6 and not di6_prev:
            host_api.log_message('[Safety] DI6 Active -> Pausing')
            host_api.set_paused(True)
        elif not di6 and di6_prev:
            host_api.log_message('[Safety] DI6 Released -> Resuming')
            host_api.set_paused(False)
        di6_prev = di6

        # DI-7: Home Logic (Level Trigger)
        if host_api.get_di(7):
             # Home First 2 Axes
             host_api.axis_move(0, 0.0, 2.0)
             host_api.axis_move(1, 0.0, 2.0)
        time.sleep(0.1)

monitor_thread = threading.Thread(target=safety_monitor, daemon=True)
monitor_thread.start()

def main():
    host_api.log_message('Starting AMR Logic...')
    if host_api.get_param('angle') <= 0:
        host_api.log_message('速度为0，请检查 angle 参数')
    host_api.axis_move(1, 90.00, host_api.get_param('angle'))
    while host_api.axis_is_moving(1):
        host_api.sleep_ms(10)
    host_api.log_message('[AMR] Waiting for DI 1...')
    while host_api.get_di(1) != True:
        host_api.sleep_ms(10)
    if host_api.get_param('height') <= 0:
        host_api.log_message('速度为0，请检查 height 参数')
    host_api.axis_move(0, 100.00, host_api.get_param('height'))
    while host_api.axis_is_moving(0):
        host_api.sleep_ms(10)
    host_api.set_do(1, True)
    host_api.log_message('[AMR] Waiting for DI 2...')
    while host_api.get_di(2) != True:
        host_api.sleep_ms(10)
    host_api.axis_move(1, 0.00, 2.00)
    while host_api.axis_is_moving(1):
        host_api.sleep_ms(10)
    host_api.set_do(2, True)
    host_api.log_message('[AMR] Waiting for DI 5...')
    while host_api.get_di(5) != True:
        host_api.sleep_ms(10)
    host_api.axis_move(0, 0.00, 2.00)
    while host_api.axis_is_moving(0):
        host_api.sleep_ms(10)
    host_api.log_message('Program Complete.')

if __name__ == '__main__':
    main()
