import host_api
import random

CELL_SIZE = 10
COLS = 40
ROWS = 30
WIDTH = COLS * CELL_SIZE
HEIGHT = ROWS * CELL_SIZE

class GameOfLife:
    def __init__(self):
        self.grid = [[0 for _ in range(COLS)] for _ in range(ROWS)]
        self.paused = False
        self.randomize()
        
    def randomize(self):
        for r in range(ROWS):
            for c in range(COLS):
                self.grid[r][c] = 1 if random.random() > 0.8 else 0

    def clear(self):
        self.grid = [[0 for _ in range(COLS)] for _ in range(ROWS)]

    def update(self):
        # Input Handling
        if host_api.is_key_down("space"):
            self.paused = not self.paused
            host_api.sleep_ms(200) # Debounce
            
        if host_api.is_key_down("up"): # Reset
            self.randomize()
            
        if host_api.is_key_down("down"): # Clear
            self.clear()

        # Mouse Drawing
        mx, my = host_api.get_mouse_pos()
        if host_api.is_mouse_down(0):
            c = int(mx / CELL_SIZE)
            r = int(my / CELL_SIZE)
            if 0 <= c < COLS and 0 <= r < ROWS:
                self.grid[r][c] = 1

        if self.paused:
            return

        # Simulation Step
        new_grid = [[0 for _ in range(COLS)] for _ in range(ROWS)]
        
        for r in range(ROWS):
            for c in range(COLS):
                neighbors = self.count_neighbors(r, c)
                state = self.grid[r][c]
                
                if state == 1 and (neighbors < 2 or neighbors > 3):
                    new_grid[r][c] = 0 # Die
                elif state == 0 and neighbors == 3:
                    new_grid[r][c] = 1 # Born
                else:
                    new_grid[r][c] = state # Stay
                    
        self.grid = new_grid

    def count_neighbors(self, r, c):
        count = 0
        for dr in [-1, 0, 1]:
            for dc in [-1, 0, 1]:
                if dr == 0 and dc == 0: continue
                nr, nc = r + dr, c + dc
                if 0 <= nr < ROWS and 0 <= nc < COLS:
                    count += self.grid[nr][nc]
        return count

    def draw(self):
        host_api.clear_screen()
        # Draw Living Cells
        for r in range(ROWS):
            for c in range(COLS):
                if self.grid[r][c] == 1:
                    # Color based on position for fun gradient
                    red = int(255 * (c / COLS))
                    green = int(255 * (r / ROWS))
                    host_api.draw_rect(c*CELL_SIZE, r*CELL_SIZE, CELL_SIZE-1, CELL_SIZE-1, red, green, 150)
                    
        # UI
        status = "PAUSED" if self.paused else "RUNNING"
        host_api.draw_text(10, HEIGHT - 20, f"Status: {status}", 255, 255, 255)
        host_api.draw_text(150, HEIGHT - 20, "Space: Pause | Up: Rand | Down: Clear | Mouse: Draw", 200, 200, 200)

def main():
    host_api.log_message("Starting Game of Life...")
    game = GameOfLife()
    
    while True:
        game.update()
        game.draw()
        host_api.sleep_ms(50) # Speed control

if __name__ == "__main__":
    main()
