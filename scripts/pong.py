import host_api
import random

# Pong Constants
WIDTH = 400
HEIGHT = 300
PADDLE_W = 10
PADDLE_H = 60
BALL_R = 5
BG_COLOR = (20, 20, 30)
PADDLE_COLOR = (200, 200, 200)
BALL_COLOR = (255, 255, 255)
SPEED = 5

class PongGame:
    def __init__(self):
        self.p1_y = HEIGHT / 2 - PADDLE_H / 2
        self.p2_y = HEIGHT / 2 - PADDLE_H / 2
        self.ball_x = WIDTH / 2
        self.ball_y = HEIGHT / 2
        
        self.ball_dx = SPEED * (1 if random.random() > 0.5 else -1)
        self.ball_dy = SPEED * (1 if random.random() > 0.5 else -1)
        
        self.score1 = 0
        self.score2 = 0
        
    def reset_ball(self):
        self.ball_x = WIDTH / 2
        self.ball_y = HEIGHT / 2
        self.ball_dx = SPEED * (1 if random.random() > 0.5 else -1)
        self.ball_dy = SPEED * (1 if random.random() > 0.5 else -1)

    def update(self):
        # Physics Step
        
        # Player 1 (Left): W/S Keys
        if host_api.is_key_down("up") and self.p1_y > 0:
            self.p1_y -= 5
        if host_api.is_key_down("down") and self.p1_y < HEIGHT - PADDLE_H:
            self.p1_y += 5
            
        # Player 2 (Right): AI (Simple tracking)
        # Add some error/delay to AI so it's beatable
        target_y = self.ball_y - PADDLE_H / 2
        if self.p2_y < target_y: self.p2_y += 3
        if self.p2_y > target_y: self.p2_y -= 3
        
        # Clamp P2
        if self.p2_y < 0: self.p2_y = 0
        if self.p2_y > HEIGHT - PADDLE_H: self.p2_y = HEIGHT - PADDLE_H
        
        # Ball Movement
        self.ball_x += self.ball_dx
        self.ball_y += self.ball_dy
        
        # Wall Collision (Top/Bottom)
        if self.ball_y <= 0 or self.ball_y >= HEIGHT:
            self.ball_dy *= -1
            
        # Paddle Collision
        # Left Paddle
        if (self.ball_x <= PADDLE_W and 
            self.p1_y <= self.ball_y <= self.p1_y + PADDLE_H):
            self.ball_dx *= -1
            self.ball_x = PADDLE_W + 1
            
        # Right Paddle
        if (self.ball_x >= WIDTH - PADDLE_W and 
            self.p2_y <= self.ball_y <= self.p2_y + PADDLE_H):
            self.ball_dx *= -1
            self.ball_x = WIDTH - PADDLE_W - 1
            
        # Score
        if self.ball_x < 0:
            self.score2 += 1
            self.reset_ball()
        if self.ball_x > WIDTH:
            self.score1 += 1
            self.reset_ball()

    def draw(self):
        host_api.clear_screen()
        host_api.draw_rect(0, 0, WIDTH, HEIGHT, *BG_COLOR)
        
        # Net
        for i in range(0, HEIGHT, 20):
            host_api.draw_rect(WIDTH/2 - 1, i, 2, 10, 50, 50, 50)
            
        # Paddles
        host_api.draw_rect(0, self.p1_y, PADDLE_W, PADDLE_H, *PADDLE_COLOR)
        host_api.draw_rect(WIDTH - PADDLE_W, self.p2_y, PADDLE_W, PADDLE_H, *PADDLE_COLOR)
        
        # Ball
        host_api.draw_circle(self.ball_x, self.ball_y, BALL_R, *BALL_COLOR)
        
        # Score Text (Centered)
        score_str = f"{self.score1}  {self.score2}"
        host_api.draw_text(WIDTH/2 - 20, 20, score_str, 200, 200, 200)

def main():
    host_api.log_message("Starting Pong...")
    host_api.log_message("Control: Left Paddle (W/S or Up/Down). Right Paddle is AI.")
    
    game = PongGame()
    
    while True:
        game.update()
        game.draw()
        
        # Export vars for debugging
        ball_pos = (int(game.ball_x), int(game.ball_y))
        
        host_api.sleep_ms(33) # ~30 FPS

if __name__ == "__main__":
    main()
