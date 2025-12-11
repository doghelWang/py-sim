import host_api
import random

# Constants
GRID_SIZE = 20
GRID_W = 400 // GRID_SIZE
GRID_H = 300 // GRID_SIZE
SLEEP_MS = 300

class SnakeGame:
    def __init__(self):
        self.snake = [(5, 5), (4, 5), (3, 5)]
        self.direction = (1, 0) # Right
        self.food = self.spawn_food()
        self.game_over = False
        self.score = 0
        
    def spawn_food(self):
        while True:
            # host_api.get_random_data returns [0-100], we need grid logic
            # Simulating random int via API (or just internal Python random)
            # Since we have full Python, we just use random
            x = random.randint(0, GRID_W - 1)
            y = random.randint(0, GRID_H - 1)
            if (x, y) not in self.snake:
                return (x, y)

    def handle_input(self):
        if host_api.is_key_down("up") and self.direction != (0, 1):
            self.direction = (0, -1)
        elif host_api.is_key_down("down") and self.direction != (0, -1):
            self.direction = (0, 1)
        elif host_api.is_key_down("left") and self.direction != (1, 0):
            self.direction = (-1, 0)
        elif host_api.is_key_down("right") and self.direction != (-1, 0):
            self.direction = (1, 0)

    def update(self):
        head = self.snake[0]
        new_head = (head[0] + self.direction[0], head[1] + self.direction[1])
        
        # Wall Collision
        if new_head[0] < 0 or new_head[0] >= GRID_W or new_head[1] < 0 or new_head[1] >= GRID_H:
            self.game_over = True
            return
            
        # Self Collision
        if new_head in self.snake:
            self.game_over = True
            return
            
        self.snake.insert(0, new_head)
        
        # Eat Food
        if new_head == self.food:
            self.score += 10
            self.food = self.spawn_food()
        else:
            self.snake.pop()

    def draw(self):
        host_api.clear_screen()
        
        # Draw Food (Red Circle)
        fx, fy = self.food
        host_api.draw_circle(fx * GRID_SIZE + GRID_SIZE/2, fy * GRID_SIZE + GRID_SIZE/2, GRID_SIZE/2 - 2, 255, 50, 50)
        
        # Draw Snake (Green Rects)
        for i, (sx, sy) in enumerate(self.snake):
            color_g = 255 if i == 0 else 200
            host_api.draw_rect(sx * GRID_SIZE + 1, sy * GRID_SIZE + 1, GRID_SIZE - 2, GRID_SIZE - 2, 50, color_g, 50)

def main():
    host_api.log_message("Starting Snake Game...")
    host_api.log_message("Control: Arrow Keys / Variable Watch: Check 'score'")
    
    game = SnakeGame()
    
    while not game.game_over:
        game.handle_input()
        game.update()
        game.draw()
        
        # Expose score to C++ Variable Watch
        current_score = game.score
        head_pos = game.snake[0]
        
        host_api.sleep_ms(SLEEP_MS)
        
    host_api.clear_screen()
    host_api.draw_rect(0, 0, 400, 300, 50, 0, 0) # Dark Red BG
    host_api.log_message(f"Game Over! Final Score: {game.score}")

if __name__ == "__main__":
    main()
