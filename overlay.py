import ctypes
import ctypes.wintypes
import struct
import numpy as np

# DirectX 11 GUIDs
IID_ID3D11Device = "6b3f3d7a-2d9e-4b6f-8a1c-3d9e8f7a6b5c"
IID_ID3D11DeviceContext = "8c5a3d7e-2b9f-4e6a-8d1c-3f9e7a5b6c2d"
IID_IDXGISwapChain = "7a3f5d8e-2c9b-4e6a-8d1f-3c9e7a5b6d2f"

class DX11Overlay:
    def __init__(self, hwnd, width, height):
        self.hwnd = hwnd
        self.width = width
        self.height = height
        self.device = None
        self.context = None
        self.swap_chain = None
        self.render_target_view = None
        
    def initialize(self):
        """Initialize DirectX 11 for overlay rendering"""
        try:
            # Load DXGI and D3D11
            dxgi = ctypes.windll.dxgi
            d3d11 = ctypes.windll.d3d11
            
            # Create swap chain description
            class DXGI_SWAP_CHAIN_DESC(ctypes.Structure):
                _fields_ = [
                    ("BufferDesc_Width", ctypes.c_uint),
                    ("BufferDesc_Height", ctypes.c_uint),
                    ("BufferDesc_RefreshRate_Numerator", ctypes.c_uint),
                    ("BufferDesc_RefreshRate_Denominator", ctypes.c_uint),
                    ("BufferDesc_Format", ctypes.c_uint),
                    ("BufferDesc_ScanlineOrdering", ctypes.c_uint),
                    ("BufferDesc_Scaling", ctypes.c_uint),
                    ("SampleDesc_Count", ctypes.c_uint),
                    ("SampleDesc_Quality", ctypes.c_uint),
                    ("BufferUsage", ctypes.c_uint),
                    ("BufferCount", ctypes.c_uint),
                    ("OutputWindow", ctypes.c_void_p),
                    ("Windowed", ctypes.c_int),
                    ("SwapEffect", ctypes.c_uint),
                    ("Flags", ctypes.c_uint)
                ]
            
            desc = DXGI_SWAP_CHAIN_DESC()
            desc.BufferDesc_Width = self.width
            desc.BufferDesc_Height = self.height
            desc.BufferDesc_Format = 87  # DXGI_FORMAT_B8G8R8A8_UNORM
            desc.SampleDesc_Count = 1
            desc.BufferUsage = 0x20  # DXGI_USAGE_RENDER_TARGET_OUTPUT
            desc.BufferCount = 1
            desc.OutputWindow = self.hwnd
            desc.Windowed = 1
            desc.SwapEffect = 0  # DXGI_SWAP_EFFECT_DISCARD
            
            # Create device and swap chain
            feature_level = ctypes.c_uint()
            device_ptr = ctypes.c_void_p()
            context_ptr = ctypes.c_void_p()
            swap_chain_ptr = ctypes.c_void_p()
            
            result = d3d11.D3D11CreateDeviceAndSwapChain(
                None,  # pAdapter
                1,     # DriverType: D3D_DRIVER_TYPE_HARDWARE
                None,  # Software
                0,     # Flags
                None,  # pFeatureLevels
                0,     # FeatureLevels
                1,     # SDKVersion
                byref(desc),  # pSwapChainDesc
                byref(swap_chain_ptr),  # ppSwapChain
                byref(device_ptr),  # ppDevice
                byref(feature_level),  # pFeatureLevel
                byref(context_ptr)  # ppImmediateContext
            )
            
            if result == 0:  # S_OK
                self.device = device_ptr
                self.context = context_ptr
                self.swap_chain = swap_chain_ptr
                
                # Create render target view
                back_buffer = ctypes.c_void_p()
                self.swap_chain.GetBuffer(0, IID_ID3D11Texture2D, byref(back_buffer))
                
                self.device.CreateRenderTargetView(back_buffer, None, byref(self.render_target_view))
                
                return True
                
        except Exception as e:
            print(f"DX11 init error: {e}")
            
        return False
    
    def begin_scene(self):
        """Prepare for drawing"""
        if self.context:
            color = ctypes.c_float * 4
            clear_color = color(0.0, 0.0, 0.0, 0.0)  # Transparent clear
            self.context.ClearRenderTargetView(self.render_target_view, clear_color)
            
    def end_scene(self):
        """Present the frame"""
        if self.swap_chain:
            self.swap_chain.Present(1, 0)  # VSync on