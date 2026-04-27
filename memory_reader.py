import ctypes
import struct
import time
import random

class MemoryReader:
    def __init__(self, pid, driver_handle):
        self.pid = pid
        self.driver = driver_handle
        self.cache = {}
        self.cache_size = 100
        self.random_delays = True
        
    def _add_delay(self):
        """Add random delay to avoid timing analysis"""
        if self.random_delays:
            time.sleep(random.uniform(0.0005, 0.002))
    
    def read(self, address, size, use_cache=False):
        """Read memory with caching option"""
        if use_cache and address in self.cache:
            self._add_delay()
            return self.cache[address]
        
        buffer = ctypes.create_string_buffer(size)
        request = struct.pack('QIIQ', 0xDEADBEEF, self.pid, address, size)
        
        returned = ctypes.c_ulong(0)
        success = ctypes.windll.kernel32.DeviceIoControl(
            self.driver,
            0x800,  # IOCTL_READ
            request, len(request),
            buffer, size,
            ctypes.byref(returned),
            None
        )
        
        if success:
            data = buffer.raw
            
            if use_cache:
                # Cache management
                if len(self.cache) >= self.cache_size:
                    # Remove oldest entry
                    oldest = next(iter(self.cache))
                    del self.cache[oldest]
                self.cache[address] = data
            
            return data
        
        return None
    
    def read_int(self, address, use_cache=False):
        data = self.read(address, 4, use_cache)
        if data:
            return struct.unpack('I', data)[0]
        return 0
    
    def read_float(self, address, use_cache=False):
        data = self.read(address, 4, use_cache)
        if data:
            return struct.unpack('f', data)[0]
        return 0.0
    
    def read_long(self, address, use_cache=False):
        data = self.read(address, 8, use_cache)
        if data:
            return struct.unpack('Q', data)[0]
        return 0
    
    def read_vector(self, address, use_cache=False):
        data = self.read(address, 12, use_cache)
        if data:
            return struct.unpack('fff', data)
        return (0.0, 0.0, 0.0)
    
    def read_matrix4x4(self, address, use_cache=False):
        data = self.read(address, 64, use_cache)
        if data:
            return list(struct.unpack('16f', data))
        return [0.0] * 16
    
    def clear_cache(self):
        """Clear memory cache"""
        self.cache.clear()