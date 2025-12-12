import host_api
import random

WIDTH = 400
HEIGHT = 300
BIRD_R = 10
BIRD_COLOR = (255, 200, 50)
PIPE_W = 40
PIPE_GAP = 100
PIPE_SPEED = 3
GRAVITY = 0.5
JUMP_STR = -8

class FlappyBird:
    def __init__(self):
        self.reset()
        
    def reset(self):
        self.bird_y = HEIGHT / 2
        self.bird_vel = 0
        self.pipes = []
        self.score = 0
        self.game_over = False
        self.spawn_timer = 0
        
    def spawn_pipe(self):
        gap_y = random.randint(50, HEIGHT - 50 - PIPE_GAP)
        self.pipes.append({'x': WIDTH, 'gap_y': gap_y, 'scored': False})

    def update(self):
        if self.game_over:
            if host_api.is_key_down("space") or host_api.is_mouse_down(0):
                self.reset()
            return

        # Input (Space or Click)
        if host_api.is_key_down("space") or host_api.is_mouse_down(0):
            self.bird_vel = JUMP_STR
            host_api.spawn_particles(50, self.bird_y, 5, 200, 200, 200) # Puff of smoke
            
        # Physics
        self.bird_vel += GRAVITY
        self.bird_y += self.bird_vel
        
        # Floor/Ceiling
        if self.bird_y < 0: self.bird_y = 0
        if self.bird_y > HEIGHT:
            self.die()
            
        # Pipes
        self.spawn_timer += 1
        if self.spawn_timer > 90: # Every ~1.5s
            self.spawn_pipe()
            self.spawn_timer = 0
            
        for p in self.pipes:
            p['x'] -= PIPE_SPEED
            
            # Collision
            if (p['x'] < 50 + BIRD_R and p['x'] + PIPE_W > 50 - BIRD_R):
                # Inside pipe X range (Bird is at X=50)
                if (self.bird_y - BIRD_R < p['gap_y'] or 
                    self.bird_y + BIRD_R > p['gap_y'] + PIPE_GAP):
                    self.die()
            
            # Score
            if not p['scored'] and p['x'] + PIPE_W < 50:
                self.score += 1
                p['scored'] = True
                
        # Remove old pipes
        self.pipes = [p for p in self.pipes if p['x'] > -PIPE_W]
        
    def die(self):
        if not self.game_over:
            self.game_over = True
            host_api.screen_shake(15.0) # IMPACT!
            host_api.spawn_particles(50, self.bird_y, 50, 255, 50, 50) # Explosion
            
    def draw(self):
        host_api.clear_screen()
        host_api.draw_rect(0, 0, WIDTH, HEIGHT, 100, 200, 255) # Sky Blue
        
        # Draw Pipes
        for p in self.pipes:
            # Top Pipe
            host_api.draw_rect(p['x'], 0, PIPE_W, p['gap_y'], 50, 200, 50)
            # Bottom Pipe
            host_api.draw_rect(p['x'], p['gap_y'] + PIPE_GAP, PIPE_W, HEIGHT - (p['gap_y'] + PIPE_GAP), 50, 200, 50)
            
        # Draw Bird
        host_api.draw_circle(50, self.bird_y, BIRD_R, *BIRD_COLOR)
        
        # Draw Score
        host_api.draw_text(WIDTH/2, 50, str(self.score), 255, 255, 255)
        
        if self.game_over:
            host_api.draw_text(WIDTH/2 - 40, HEIGHT/2, "GAME OVER", 255, 0, 0)
            host_api.draw_text(WIDTH/2 - 60, HEIGHT/2 + 20, "Press Space", 255, 255, 255)

def main():
    host_api.log_message("Starting Flappy Bird...")
    host_api.log_message("Control: SPACE or CLICK to Jump.")
    
    game = FlappyBird()
    
    while True:
        game.update()
        game.draw()
        host_api.sleep_ms(16)

if __name__ == "__main__":
    main()
