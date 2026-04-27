#!/usr/bin/env python3
import ctypes
import ctypes.wintypes
import struct
import time
import math
import sys
import os
import random
import json
from ctypes import byref, sizeof, c_int, c_float, c_uint, c_ulonglong, c_void_p, c_wchar_p

# Windows constants
PROCESS_ALL_ACCESS = 0x1F0FFF
TH32CS_SNAPPROCESS = 0x00000002
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_SHARE_READ = 0x00000001
FILE_SHARE_WRITE = 0x00000002

# Window styles
WS_EX_LAYERED = 0x80000
WS_EX_TRANSPARENT = 0x20
WS_EX_TOPMOST = 0x8
WS_POPUP = 0x80000000
WS_VISIBLE = 0x10000000

class Vector3(ctypes.Structure):
    _fields_ = [
        ("x", c_float),
        ("y", c_float),
        ("z", c_float)
    ]

class ExternalESP:
    def __init__(self):
        self.window_width = ctypes.windll.user32.GetSystemMetrics(0)
        self.window_height = ctypes.windll.user32.GetSystemMetrics(1)
        
        self.game_pid = 0
        self.game_handle = None
        self.driver_handle = None
        self.overlay_hwnd = None
        
        # Dynamic offsets (update these per PUBG patch)
        self.offsets = {
            "GNames": 0x8C3B840,
            "GObjects": 0x91CD608,
            "LocalPlayer": 0x3BF1368,
            "EntityList": 0x4A1C3B0,
            "PlayerCount": 0x2A17E20,
            "ViewMatrix": 0x5AFFDF0,
            "ActorPos": 0x1A0,
            "ActorHealth": 0x2C0,
            "ActorMaxHealth": 0x2C4,
            "ActorName": 0x8E0,
            "ActorTeam": 0x1274,
            "ActorDormant": 0xF8,
            "SkeletalMesh": 0x420,
            "ComponentToWorld": 0x1E0
        }
        
        self.view_matrix = [0.0] * 16
        self.local_player = 0
        self.local_team = 0
        
        self.running = True
        self.show_esp = True
        self.show_boxes = True
        self.show_health = True
        self.show_distance = True
        self.show_names = True
        self.show_snaplines = True
        
        # Colors (BGR format)
        self.color_enemy = (0, 0, 255)      # Red
        self.color_teammate = (0, 255, 0)   # Green
        self.color_visible = (0, 255, 255)  # Yellow
        self.color_box = (255, 0, 0)        # Blue
        
        # Anti-detection
        self.frame_counter = 0
        self.read_errors = 0
        
    def initialize_driver(self):
        """Connect to kernel driver"""
        try:
            self.driver_handle = ctypes.windll.kernel32.CreateFileW(
                "\\\\.\\SecureBridge",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                None,
                OPEN_EXISTING,
                0,
                None
            )
            return self.driver_handle != -1 and self.driver_handle != 0
        except:
            return False
    
    def get_process_pid(self, process_name):
        """Get PID of target process"""
        kernel32 = ctypes.windll.kernel32
        
        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", ctypes.c_uint),
                ("cntUsage", ctypes.c_uint),
                ("th32ProcessID", ctypes.c_uint),
                ("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)),
                ("th32ModuleID", ctypes.c_uint),
                ("cntThreads", ctypes.c_uint),
                ("th32ParentProcessID", ctypes.c_uint),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", ctypes.c_uint),
                ("szExeFile", ctypes.c_char * 260)
            ]
        
        hSnapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
        if hSnapshot == -1:
            return 0
        
        pe32 = PROCESSENTRY32()
        pe32.dwSize = sizeof(PROCESSENTRY32)
        
        if kernel32.Process32First(hSnapshot, byref(pe32)):
            while True:
                if pe32.szExeFile.decode('utf-8').lower() == process_name.lower():
                    pid = pe32.th32ProcessID
                    kernel32.CloseHandle(hSnapshot)
                    return pid
                if not kernel32.Process32Next(hSnapshot, byref(pe32)):
                    break
        
        kernel32.CloseHandle(hSnapshot)
        return 0
    
    def read_kernel_memory(self, address, size):
        """Read memory via kernel driver"""
        if not self.driver_handle:
            return None
        
        buffer = ctypes.create_string_buffer(size)
        request = struct.pack('QIIQ', 0xDEADBEEF, self.game_pid, address, size)
        
        returned = ctypes.c_ulong(0)
        success = ctypes.windll.kernel32.DeviceIoControl(
            self.driver_handle,
            0x800,  # IOCTL_READ
            request, len(request),
            buffer, size,
            byref(returned),
            None
        )
        
        if success:
            return buffer.raw
        return None
    
    def read_int(self, address):
        data = self.read_kernel_memory(address, 4)
        if data:
            return struct.unpack('I', data)[0]
        return 0
    
    def read_float(self, address):
        data = self.read_kernel_memory(address, 4)
        if data:
            return struct.unpack('f', data)[0]
        return 0.0
    
    def read_vector3(self, address):
        data = self.read_kernel_memory(address, 12)
        if data:
            return Vector3(*struct.unpack('fff', data))
        return Vector3(0, 0, 0)
    
    def read_string(self, address, max_len=64):
        """Read string from game memory"""
        data = self.read_kernel_memory(address, max_len)
        if data:
            try:
                # Find null terminator
                end = data.find(b'\x00')
                if end > 0:
                    return data[:end].decode('utf-8', errors='ignore')
                return data.decode('utf-8', errors='ignore')
            except:
                pass
        return ""
    
    def update_view_matrix(self):
        """Read view matrix from game"""
        matrix_addr = self.read_int(self.offsets["ViewMatrix"])
        if matrix_addr:
            data = self.read_kernel_memory(matrix_addr, 64)
            if data:
                self.view_matrix = list(struct.unpack('16f', data))
    
    def world_to_screen(self, world_pos):
        """Convert 3D world position to 2D screen coordinates"""
        w = 0.0
        
        screen_x = self.view_matrix[0] * world_pos.x + \
                   self.view_matrix[1] * world_pos.y + \
                   self.view_matrix[2] * world_pos.z + \
                   self.view_matrix[3]
        
        screen_y = self.view_matrix[4] * world_pos.x + \
                   self.view_matrix[5] * world_pos.y + \
                   self.view_matrix[6] * world_pos.z + \
                   self.view_matrix[7]
        
        w = self.view_matrix[12] * world_pos.x + \
            self.view_matrix[13] * world_pos.y + \
            self.view_matrix[14] * world_pos.z + \
            self.view_matrix[15]
        
        if w < 0.01:
            return None
        
        inv_w = 1.0 / w
        screen_x *= inv_w
        screen_y *= inv_w
        
        screen_x = (self.window_width / 2) * (1 + screen_x)
        screen_y = (self.window_height / 2) * (1 - screen_y)
        
        return (int(screen_x), int(screen_y))
    
    def get_entity_list(self):
        """Get list of all entities in game"""
        entity_list_ptr = self.read_int(self.offsets["EntityList"])
        if not entity_list_ptr:
            return []
        
        entity_count = self.read_int(self.offsets["PlayerCount"])
        if entity_count > 100:
            entity_count = 100
        
        entities = []
        for i in range(entity_count):
            entity_ptr = self.read_int(entity_list_ptr + (i * 8))
            if entity_ptr and entity_ptr > 0x10000:
                entities.append(entity_ptr)
        
        return entities
    
    def is_entity_dormant(self, entity):
        """Check if entity is dormant (not updating)"""
        dormant = self.read_int(entity + self.offsets["EntityDormant"])
        return dormant == 1
    
    def get_entity_position(self, entity):
        """Get entity world position"""
        # Try root component first
        root_component = self.read_int(entity + 0x1A0)
        if root_component:
            pos = self.read_vector3(root_component + 0x140)
            if pos.x != 0 or pos.y != 0 or pos.z != 0:
                return pos
        
        # Fallback to actor position
        return self.read_vector3(entity + self.offsets["ActorPos"])
    
    def get_entity_health(self, entity):
        """Get entity health"""
        return self.read_float(entity + self.offsets["ActorHealth"])
    
    def get_entity_max_health(self, entity):
        """Get entity max health"""
        return self.read_float(entity + self.offsets["ActorMaxHealth"])
    
    def get_entity_name(self, entity):
        """Get entity name from GNames"""
        # This requires GNames parsing
        name_ptr = self.read_int(entity + self.offsets["ActorName"])
        if name_ptr:
            return self.read_string(name_ptr, 32)
        return "Player"
    
    def get_entity_team(self, entity):
        """Get entity team ID"""
        return self.read_int(entity + self.offsets["ActorTeam"])
    
    def get_bone_position(self, mesh, bone_index):
        """Get bone position for skeletal mesh"""
        bone_array = self.read_int(mesh + 0x5A0)
        if bone_array:
            component_transform = bone_array + (bone_index * 0x60)
            return self.read_vector3(component_transform + 0x10)
        return None
    
    def distance_between(self, pos1, pos2):
        """Calculate distance between two vectors"""
        dx = pos1.x - pos2.x
        dy = pos1.y - pos2.y
        dz = pos1.z - pos2.z
        return math.sqrt(dx*dx + dy*dy + dz*dz) / 100.0  # Convert to meters
    
    def draw_text(self, text, x, y, color):
        """Draw text on overlay"""
        # GDI drawing (implemented in overlay window)
        pass
    
    def draw_box(self, x, y, width, height, color):
        """Draw ESP box"""
        pass
    
    def draw_line(self, x1, y1, x2, y2, color):
        """Draw line"""
        pass
    
    def draw_health_bar(self, x, y, width, height, health_percent, color):
        """Draw health bar"""
        pass
    
    def draw_esp(self):
        """Main ESP drawing logic"""
        if not self.show_esp:
            return
        
        # Get local player
        self.local_player = self.read_int(self.offsets["LocalPlayer"])
        if not self.local_player:
            return
        
        self.local_team = self.get_entity_team(self.local_player)
        local_pos = self.get_entity_position(self.local_player)
        
        entities = self.get_entity_list()
        
        for entity in entities:
            if entity == self.local_player:
                continue
            
            team = self.get_entity_team(entity)
            if team == self.local_team:
                continue
            
            if self.is_entity_dormant(entity):
                continue
            
            entity_pos = self.get_entity_position(entity)
            if entity_pos.x == 0 and entity_pos.y == 0 and entity_pos.z == 0:
                continue
            
            distance = self.distance_between(local_pos, entity_pos)
            if distance > 400:
                continue
            
            health = self.get_entity_health(entity)
            max_health = self.get_entity_max_health(entity)
            health_percent = (health / max_health) * 100 if max_health > 0 else 100
            
            # Get head and feet positions for box
            mesh = self.read_int(entity + self.offsets["SkeletalMesh"])
            if mesh:
                head_pos = self.get_bone_position(mesh, 68)  # Head bone index
                if head_pos:
                    screen_head = self.world_to_screen(head_pos)
                    screen_feet = self.world_to_screen(entity_pos)
                    
                    if screen_head and screen_feet:
                        height = abs(screen_feet[1] - screen_head[1])
                        width = height * 0.6
                        x = screen_head[0] - (width / 2)
                        y = screen_head[1]
                        
                        # Determine color based on visibility
                        color = self.color_enemy
                        
                        # Draw box
                        if self.show_boxes:
                            self.draw_box(x, y, width, height, color)
                        
                        # Draw health bar
                        if self.show_health:
                            bar_width = width
                            bar_height = height * (health_percent / 100)
                            self.draw_health_bar(x, y + height - bar_height, bar_width, bar_height, health_percent, (0, 255, 0))
                        
                        # Draw distance
                        if self.show_distance:
                            dist_text = f"{int(distance)}m"
                            self.draw_text(dist_text, x, y - 15, (255, 255, 255))
                        
                        # Draw name
                        if self.show_names:
                            name = self.get_entity_name(entity)
                            self.draw_text(name, x, y - 30, (255, 255, 255))
                        
                        # Draw snapline to crosshair
                        if self.show_snaplines:
                            center_x = self.window_width / 2
                            center_y = self.window_height / 2
                            self.draw_line(center_x, center_y, screen_feet[0], screen_feet[1], color)
        
        # Update view matrix every frame
        self.update_view_matrix()
        
        # Anti-detection: random frame delay
        time.sleep(random.uniform(0.008, 0.012))
        self.frame_counter += 1
    
    def create_overlay_window(self):
        """Create transparent overlay window for ESP"""
        wndclass = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_long)
        
        def wndproc(hwnd, msg, wparam, lparam):
            if msg == 0x0002:  # WM_DESTROY
                ctypes.windll.user32.PostQuitMessage(0)
                return 0
            return ctypes.windll.user32.DefWindowProcW(hwnd, msg, wparam, lparam)
        
        wndproc_callback = wndproc
        
        # Register window class
        classname = "ESP_Overlay_Class"
        wndclass_atom = ctypes.windll.user32.RegisterClassW(ctypes.byref(
            type('WNDCLASS', (), {
                'style': 0,
                'lpfnWndProc': wndproc_callback,
                'cbClsExtra': 0,
                'cbWndExtra': 0,
                'hInstance': None,
                'hIcon': None,
                'hCursor': ctypes.windll.user32.LoadCursorW(None, 32512),  # IDC_ARROW
                'hbrBackground': None,
                'lpszMenuName': None,
                'lpszClassName': classname
            })()
        ))
        
        # Create window
        self.overlay_hwnd = ctypes.windll.user32.CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
            classname,
            "ESP_Overlay",
            WS_POPUP | WS_VISIBLE,
            0, 0, self.window_width, self.window_height,
            None, None, None, None
        )
        
        # Make window transparent
        ctypes.windll.user32.SetLayeredWindowAttributes(self.overlay_hwnd, 0, 0, 0x00000001)  # LWA_ALPHA
        
        return self.overlay_hwnd != 0
    
    def message_loop(self):
        """Windows message loop for overlay"""
        msg = ctypes.wintypes.MSG()
        while self.running:
            if ctypes.windll.user32.PeekMessageW(byref(msg), None, 0, 0, 1):
                ctypes.windll.user32.TranslateMessage(byref(msg))
                ctypes.windll.user32.DispatchMessageW(byref(msg))
            
            # Render ESP
            self.draw_esp()
            
            # Check for hotkey to toggle ESP
            if ctypes.windll.user32.GetAsyncKeyState(0x71) & 0x8000:  # F2 key
                self.show_esp = not self.show_esp
                time.sleep(0.3)
            
            if ctypes.windll.user32.GetAsyncKeyState(0x72) & 0x8000:  # F3 key
                self.show_boxes = not self.show_boxes
                time.sleep(0.3)
            
            if ctypes.windll.user32.GetAsyncKeyState(0x73) & 0x8000:  # F4 key
                self.show_health = not self.show_health
                time.sleep(0.3)
            
            if ctypes.windll.user32.GetAsyncKeyState(0x74) & 0x8000:  # F5 key
                self.show_distance = not self.show_distance
                time.sleep(0.3)
            
            if ctypes.windll.user32.GetAsyncKeyState(0x75) & 0x8000:  # F6 key
                self.show_names = not self.show_names
                time.sleep(0.3)
            
            if ctypes.windll.user32.GetAsyncKeyState(0x2E) & 0x8000:  # Delete key
                self.running = False
                break
    
    def run(self):
        """Main entry point"""
        print("[*] Initializing External ESP...")
        
        # Get PUBG process
        self.game_pid = self.get_process_pid("TslGame.exe")
        if not self.game_pid:
            print("[!] PUBG not found. Waiting...")
            # Wait for PUBG to start
            while not self.game_pid:
                time.sleep(2)
                self.game_pid = self.get_process_pid("TslGame.exe")
        
        print(f"[+] Found PUBG with PID: {self.game_pid}")
        
        # Connect to driver
        if not self.initialize_driver():
            print("[!] Failed to connect to kernel driver. Make sure driver is loaded.")
            print("[*] Attempting to load driver...")
            # Try to load driver automatically
            os.system("sc start SecureBridge")
            time.sleep(1)
            if not self.initialize_driver():
                print("[X] Cannot connect to driver. Exiting.")
                return
        
        print("[+] Connected to kernel driver")
        
        # Create overlay window
        if not self.create_overlay_window():
            print("[!] Failed to create overlay window")
            return
        
        print("[+] Overlay window created")
        print("[*] ESP Active - F2: Toggle ESP, F3: Toggle Boxes, F4: Toggle Health, F5: Toggle Distance, F6: Toggle Names, DEL: Exit")
        
        # Start message loop
        self.message_loop()
        
        print("[*] Exiting...")
        if self.driver_handle:
            ctypes.windll.kernel32.CloseHandle(self.driver_handle)

if __name__ == "__main__":
    esp = ExternalESP()
    esp.run()