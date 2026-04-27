import math

class WorldToScreen:
    def __init__(self, screen_width, screen_height):
        self.screen_width = screen_width
        self.screen_height = screen_height
        
    def transform(self, world_pos, view_matrix):
        """Transform 3D world position to 2D screen coordinates"""
        x, y, z = world_pos
        
        # Matrix multiplication
        screen_x = view_matrix[0] * x + view_matrix[1] * y + view_matrix[2] * z + view_matrix[3]
        screen_y = view_matrix[4] * x + view_matrix[5] * y + view_matrix[6] * z + view_matrix[7]
        w = view_matrix[12] * x + view_matrix[13] * y + view_matrix[14] * z + view_matrix[15]
        
        # Check if behind camera
        if w < 0.01:
            return None
        
        # Perspective division
        inv_w = 1.0 / w
        screen_x *= inv_w
        screen_y *= inv_w
        
        # Convert to screen coordinates
        screen_x = (self.screen_width / 2) * (1 + screen_x)
        screen_y = (self.screen_height / 2) * (1 - screen_y)
        
        # Check if on screen
        if 0 <= screen_x <= self.screen_width and 0 <= screen_y <= self.screen_height:
            return (int(screen_x), int(screen_y))
        
        return None
    
    def get_box_bounds(self, head_pos, feet_pos, view_matrix):
        """Get ESP box bounds from head and feet positions"""
        screen_head = self.transform(head_pos, view_matrix)
        screen_feet = self.transform(feet_pos, view_matrix)
        
        if screen_head and screen_feet:
            height = abs(screen_feet[1] - screen_head[1])
            width = height * 0.6
            x = screen_head[0] - (width / 2)
            y = screen_head[1]
            return (x, y, width, height)
        
        return None