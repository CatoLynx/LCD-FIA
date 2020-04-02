import serial
import spidev
import time

from PIL import Image, ImageSequence

class FIA:
    UART_CMD_NULL = 0x00
    UART_CMD_MCU_RESET = 0x01
    
    UART_CMD_SET_BACKLIGHT_STATE = 0x10
    UART_CMD_GET_BACKLIGHT_STATE = 0x11
    UART_CMD_SET_BACKLIGHT_BASE_BRIGHTNESS = 0x12
    UART_CMD_GET_BACKLIGHT_BASE_BRIGHTNESS = 0x13
    UART_CMD_GET_BACKLIGHT_BRIGHTNESS = 0x14
    UART_CMD_GET_ENV_BRIGHTNESS = 0x15
    
    UART_CMD_SET_HEATERS_STATE = 0x20
    UART_CMD_GET_HEATERS_STATE = 0x21
    UART_CMD_SET_CIRCULATION_FANS_STATE = 0x22
    UART_CMD_GET_CIRCULATION_FANS_STATE = 0x23
    UART_CMD_SET_HEAT_EXCHANGER_FAN_STATE = 0x24
    UART_CMD_GET_HEAT_EXCHANGER_FAN_STATE = 0x25
    UART_CMD_SET_BACKLIGHT_BALLAST_FANS_STATE = 0x26
    UART_CMD_GET_BACKLIGHT_BALLAST_FANS_STATE = 0x27
    
    UART_CMD_GET_DOOR_STATES = 0x30
    
    UART_CMD_GET_TEMPERATURES = 0x40
    UART_CMD_GET_HUMIDITY = 0x41
    
    def __init__(self, uart_port, spi_port, uart_baud = 115200, uart_timeout = 1.0, spi_clock = 5000000, width = 480, height = 128, panel_width = 96, panel_height = 64):
        self.uart = serial.Serial(uart_port, baudrate=uart_baud, timeout=uart_timeout)
        self.spi = spidev.SpiDev()
        self.spi.open(*spi_port)
        self.spi.max_speed_hz = spi_clock
        self.width = width
        self.height = height
        self.panel_width = panel_width
        self.panel_height = panel_height
    
    def send_uart_command_raw(self, raw_command):
        # Just send a raw UART command
        self.uart.write(bytearray(raw_command))
    
    def calculate_uart_checksum(self, data):
        checksum = 0x7F
        for byte in data:
            checksum ^= byte
        return checksum
    
    def read_uart_response(self):
        header = self.uart.read(2)
        if header[0] != 0xFF:
            return None
        length = header[1]
        payload = self.uart.read(length)
        if len(payload) != length:
            print("err len")
            return None
        checksum = payload[-1]
        if checksum != self.calculate_uart_checksum(payload[:-1]):
            print("err chk")
            return None
        #print(header + payload)
        return bytearray(payload[:-1])
    
    def send_uart_command(self, command, data = [], expect_response = True):
        # Send UART command and return response
        assert len(data) <= 254
        raw_command = [0xFF, len(data) + 2, command]
        raw_command += data
        checksum = self.calculate_uart_checksum([command] + data)
        raw_command.append(checksum)
        self.send_uart_command_raw(raw_command)
        if expect_response:
            return self.read_uart_response()
        else:
            return None
    
    def twos_comp(self, val, bits):
        if val < 0:
            return val + 2**bits
        else:
            return val
    
    def inv_twos_comp(self, val, bits):
        if val & (1 << (bits-1)):
            return val - 2**bits
        else:
            return val
    
    def null_cmd(self):
        resp = self.send_uart_command(self.UART_CMD_NULL)
    
    def mcu_reset(self):
        resp = self.send_uart_command(self.UART_CMD_MCU_RESET, expect_response=False)
    
    def set_backlight_state(self, state):
        resp = self.send_uart_command(self.UART_CMD_SET_BACKLIGHT_STATE, [state])
    
    def get_backlight_state(self):
        resp = self.send_uart_command(self.UART_CMD_GET_BACKLIGHT_STATE)
        return resp[0]
    
    def set_backlight_base_brightness(self, side_a, side_b):
        side_a = self.twos_comp(side_a, 16)
        side_b = self.twos_comp(side_b, 16)
        params = [side_a >> 8, side_a & 0xFF, side_b >> 8, side_b & 0xFF]
        resp = self.send_uart_command(self.UART_CMD_SET_BACKLIGHT_BASE_BRIGHTNESS, params)
    
    def get_backlight_base_brightness(self):
        resp = self.send_uart_command(self.UART_CMD_GET_BACKLIGHT_BASE_BRIGHTNESS)
        side_a = self.inv_twos_comp((resp[0] << 8) | resp[1], 16)
        side_b = self.inv_twos_comp((resp[2] << 8) | resp[3], 16)
        return side_a, side_b
    
    def get_backlight_brightness(self):
        resp = self.send_uart_command(self.UART_CMD_GET_BACKLIGHT_BRIGHTNESS)
        side_a = (resp[0] << 8) | resp[1]
        side_b = (resp[2] << 8) | resp[3]
        return side_a, side_b
    
    def get_env_brightness(self):
        resp = self.send_uart_command(self.UART_CMD_GET_ENV_BRIGHTNESS)
        side_a = (resp[0] << 8) | resp[1]
        side_b = (resp[2] << 8) | resp[3]
        return side_a, side_b
    
    def set_heaters_state(self, state):
        resp = self.send_uart_command(self.UART_CMD_SET_HEATERS_STATE, [state])
    
    def get_heaters_state(self):
        resp = self.send_uart_command(self.UART_CMD_GET_HEATERS_STATE)
        return resp[0]
    
    def set_circulation_fans_state(self, state):
        resp = self.send_uart_command(self.UART_CMD_SET_CIRCULATION_FANS_STATE, [state])
    
    def get_circulation_fans_state(self):
        resp = self.send_uart_command(self.UART_CMD_GET_CIRCULATION_FANS_STATE)
        return resp[0]
    
    def set_heat_exchanger_fan_state(self, state):
        resp = self.send_uart_command(self.UART_CMD_SET_HEAT_EXCHANGER_FAN_STATE, [state])
    
    def get_heat_exchanger_fan_state(self):
        resp = self.send_uart_command(self.UART_CMD_GET_HEAT_EXCHANGER_FAN_STATE)
        return resp[0]
    
    def set_backlight_ballast_fans_state(self, state):
        resp = self.send_uart_command(self.UART_CMD_SET_BACKLIGHT_BALLAST_FANS_STATE, [state])
    
    def get_backlight_ballast_fans_state(self):
        resp = self.send_uart_command(self.UART_CMD_GET_BACKLIGHT_BALLAST_FANS_STATE)
        return resp[0]
    
    def get_door_states(self):
        resp = self.send_uart_command(self.UART_CMD_GET_DOOR_STATES)
        return resp[0]
    
    def get_temperatures(self):
        resp = self.send_uart_command(self.UART_CMD_GET_TEMPERATURES)
        t_ext1 = ((resp[0] << 8) | resp[1]) / 100
        t_ext2 = ((resp[2] << 8) | resp[3]) / 100
        t_board = ((resp[4] << 8) | resp[5]) / 100
        t_mcu = ((resp[6] << 8) | resp[7]) / 100
        return t_ext1, t_ext2, t_board, t_mcu

    def get_humidity(self):
        resp = self.send_uart_command(self.UART_CMD_GET_HUMIDITY)
        humidity = ((resp[0] << 8) | resp[1]) / 100
        return humidity
    
    def flatten_img(self, img, panel_height = None):
        # if panel_height is given, split the image into rows of said height
        # and flatten each row separately
        pixels = img.load()
        width, height = img.size

        if panel_height:
            num_rows = height // panel_height
        else:
            num_rows = 1
            panel_height = height
        
        num_hp_w = width // self.panel_width
        num_hp_h = panel_height // 32
        flattened_imgs = []
        flattened_row_width = num_hp_w * num_hp_h * self.panel_width
        for row_index in range(num_rows):
            y_base = row_index * panel_height
            row_img = img.crop((0, y_base, width-1, y_base+panel_height))
            
            out = Image.new('L', (flattened_row_width, 32))
            
            x_out = out.size[0] - self.panel_width
            for yp in range(num_hp_h):
                if not yp % 2:
                    r = range(num_hp_w)
                    rotate = False
                else:
                    r = range(num_hp_w-1, -1, -1)
                    rotate = True
                
                for xp in r:
                    x = width - xp * self.panel_width - self.panel_width
                    y = yp * 32
                    if rotate:
                        out.paste(row_img.crop((x, y, x+self.panel_width, y+32)).rotate(180), (x_out, 0))
                    else:
                        out.paste(row_img.crop((x, y, x+self.panel_width, y+32)), (x_out, 0))
                    x_out -= self.panel_width
            flattened_imgs.append(out)
        
        out = Image.new('L', (flattened_row_width * len(flattened_imgs), 32))
        for i, img in enumerate(flattened_imgs):
            out.paste(img, (flattened_row_width * i, 0))
        return out
    
    def img_to_array(self, img):
        pixels = img.load()
        width, height = img.size

        output = []

        for x in range(width):
            for y_byte in range(0, height, 8):
                byte = 0x00
                for y_bit in range(8):
                    y = y_byte + y_bit
                    state = pixels[x, y] > 64
                    if state:
                        byte |= (1 << y_bit)
                output.append(byte)
        
        return output, width, height
    
    def send_array(self, array):
        length = len(array)
        self.spi.writebytes2(bytearray(array))

    def send_image(self, img):
        if not isinstance(img, Image.Image):
            img = Image.open(img)
        
        img = img.convert('L')
        width, height = img.size
        if width > self.width and height > self.height:
            img = img.crop((0, 0, self.width, self.height))
        else:
            img2 = Image.new('L', (self.width, self.height), 'black')
            img2.paste(img, (0, 0))
            img = img2
        
        flattened = self.flatten_img(img, self.panel_height)
        array, width, height = self.img_to_array(flattened)
        self.send_array(array)
    
    def send_gif(self, img):
        if not isinstance(img, Image.Image):
            img = Image.open(img)
        
        width, height = img.size
        duration = img.info.get('duration', 1000)
        
        frames = []
        for frame in ImageSequence.Iterator(img):
            if width > self.width and height > self.height:
                frame = frame.crop((0, 0, self.width, self.height)).convert('L')
                frames.append(frame)
            else:
                tmp = Image.new('L', (self.width, self.height), 'black')
                tmp.paste(frame, (0, 0))
                frames.append(tmp)
        
        last_frame_time = 0
        cur_frame = 0
        while True:
            now = time.time()
            if now - last_frame_time >= duration / 1000:
                self.send_image(frames[cur_frame])
                last_frame_time = now
                cur_frame += 1
            if cur_frame >= len(frames):
                cur_frame = 0