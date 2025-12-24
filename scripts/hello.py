import host
import time

print("Hello from Python")
host.set_twist(0.2, 0, 0.5)
time.sleep(2)
host.set_twist(0,0,0)
print("Done")
