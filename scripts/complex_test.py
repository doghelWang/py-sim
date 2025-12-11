import host_api
import time
import random

class DataProcessor:
    def __init__(self, name):
        self.name = name
        self.processed_count = 0
        self.log_file = "process_log.txt"

    def run_batch(self, batch_id):
        host_api.log_message(f"Processor {self.name}: Starting Batch {batch_id}")
        
        # File I/O
        host_api.write_file(self.log_file, f"Batch {batch_id} started\n")
        
        # Heavy Compute
        host_api.log_message("Starting Heavy Computation...")
        prime = host_api.compute_prime(1000) # Should take a bit of time
        host_api.log_message(f"Computed 1000th prime: {prime}")
        
        # Recursion test (deep stack)
        result = self.recursive_op(5)
        
        # Data generation
        data = host_api.get_random_data(5)
        avg = sum(data) / len(data)
        
        self.processed_count += 1
        host_api.log_message(f"Batch {batch_id} Complete. Avg Data: {avg:.2f}")

    def recursive_op(self, n):
        if n <= 0: return 0
        # Sleep to allow easy pausing inside recursion
        host_api.sleep_ms(200) 
        val = n + self.recursive_op(n-1)
        return val

def main():
    processor = DataProcessor("Unit-A")
    
    host_api.log_message("Initializing Complex Workflow...")
    host_api.sleep_ms(500)
    
    for i in range(1, 100):
        # Local variables to inspect
        status = "Active"
        iteration = i
        
        processor.run_batch(i)
        
        host_api.sleep_ms(1000)
        
    host_api.log_message("Workflow Complete.")

if __name__ == "__main__":
    main()
