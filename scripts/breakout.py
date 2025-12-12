import host_api
import math

# Config
WIDTH = 400
HEIGHT = 300
PADDLE_W = 60
PADDLE_H = 10
BALL_R = 4
ROWS = 5
COLS = 8
BRICK_W = WIDTH // COLS
BRICK_H = 20

class Breakout:
    def __init__(self):
        self.paddle_x = WIDTH / 2 - PADDLE_W / 2
        self.ball_x = WIDTH / 2
        self.ball_y = HEIGHT - 40
        self.ball_dx = 4
        self.ball_dy = -4
        self.score = 0
        self.lives = 3
        
        # Init Bricks
        self.bricks = []
        for r in range(ROWS):
            for c in range(COLS):
                self.bricks.append({
                    'x': c * BRICK_W,
                    'y': r * BRICK_H + 30, # Offset from top
                    'active': True,
                    'color': self.get_row_color(r)
                })

    def get_row_color(self, row):
        colors = [
            (255, 50, 50),   # Red
            (255, 150, 50),  # Orange
            (255, 255, 50),  # Yellow
            (50, 255, 50),   # Green
            (50, 50, 255)    # Blue
        ]
        return colors[row % len(colors)]

    def update(self):
        # Paddle Control (Mouse)
        mx, my = host_api.get_mouse_pos()
        self.paddle_x = mx - PADDLE_W / 2
        
        # Clamp Paddle
        if self.paddle_x < 0: self.paddle_x = 0
        if self.paddle_x > WIDTH - PADDLE_W: self.paddle_x = WIDTH - PADDLE_W
        
        # Ball Movement
        self.ball_x += self.ball_dx
        self.ball_y += self.ball_dy
        
        # Wall Collision
        if self.ball_x <= 0 or self.ball_x >= WIDTH:
            self.ball_dx *= -1
        if self.ball_y <= 0:
            self.ball_dy *= -1
            
        # Paddle Collision
        if (self.ball_y >= HEIGHT - 30 - PADDLE_H and 
            self.ball_y <= HEIGHT - 30 and
            self.ball_x >= self.paddle_x and 
            self.ball_x <= self.paddle_x + PADDLE_W):
            self.ball_dy *= -1
            # Add some English based on hit position
            center = self.paddle_x + PADDLE_W/2
            offset = (self.ball_x - center) / (PADDLE_W/2)
            self.ball_dx = offset * 5
            
        # Brick Collision
        ball_rect = (self.ball_x - BALL_R, self.ball_y - BALL_R, BALL_R*2, BALL_R*2)
        
        for brick in self.bricks:
            if not brick['active']: continue
            
            # Simple Rect Collision
            if (self.ball_x > brick['x'] and self.ball_x < brick['x'] + BRICK_W and
                self.ball_y > brick['y'] and self.ball_y < brick['y'] + BRICK_H):
                brick['active'] = False
                self.ball_dy *= -1
                self.score += 10
                break
                
        # Death
        if self.ball_y > HEIGHT:
            self.lives -= 1
            if self.lives > 0:
                self.ball_x = WIDTH / 2
                self.ball_y = HEIGHT - 40
                self.ball_dy = -4
                host_api.sleep_ms(1000)
            else:
                self.reset_game()

    def reset_game(self):
        self.score = 0
        self.lives = 3
        for b in self.bricks: b['active'] = True
        self.ball_x = WIDTH / 2
        self.ball_y = HEIGHT - 40
        self.ball_dy = -4

    def draw(self):
        host_api.clear_screen()
        host_api.draw_rect(0, 0, WIDTH, HEIGHT, 10, 10, 15)
        
        # Draw Bricks
        for brick in self.bricks:
            if brick['active']:
                host_api.draw_rect(brick['x']+1, brick['y']+1, BRICK_W-2, BRICK_H-2, *brick['color'])
                
        # Draw Paddle
        host_api.draw_rect(self.paddle_x, HEIGHT - 30, PADDLE_W, PADDLE_H, 100, 200, 255)
        
        # Draw Ball
        host_api.draw_circle(self.ball_x, self.ball_y, BALL_R, 255, 255, 255)
        
        # Draw Text
        host_api.draw_text(10, 5, f"Score: {self.score}", 255, 255, 255)
        host_api.draw_text(WIDTH - 80, 5, f"Lives: {self.lives}", 255, 50, 50)

def main():
    host_api.log_message("Starting Breakout...")
    host_api.log_message("Control: MOUSE to move paddle.")
    
    game = Breakout()
    
    while True:
        game.update()
        game.draw()
        host_api.sleep_ms(16) # ~60 FPS

if __name__ == "__main__":
    main()
