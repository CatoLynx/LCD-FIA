import serial
import time

# For real hardware, only available on RasPi
try:
    import spidev
    _HAS_SPIDEV = True
except ImportError:
    _HAS_SPIDEV = False

from PIL import Image, ImageSequence

# For emulator
import threading
import numpy as np
import tkinter as tk
from PIL import ImageTk


class FIAError(Exception):
    pass


class FIA:
    SIDE_A = 0x01
    SIDE_B = 0x02
    SIDE_BOTH = 0x03
    
    BUF_SCROLL = 0x80
    BUF_MASK = 0x40
    BUF_DYN = 0x20
    
    SCROLL_BUF_ERR_MASK = 0x10
    SCROLL_BUF_ERR_SIZE = 1
    SCROLL_BUF_ERR_COUNT = 2
    
    UART_CMD_NULL = 0x00
    UART_CMD_MCU_RESET = 0x01
    
    UART_CMD_SET_BACKLIGHT_STATE = 0x10
    UART_CMD_GET_BACKLIGHT_STATE = 0x11
    UART_CMD_SET_BACKLIGHT_BASE_BRIGHTNESS = 0x12
    UART_CMD_GET_BACKLIGHT_BASE_BRIGHTNESS = 0x13
    UART_CMD_GET_BACKLIGHT_BRIGHTNESS = 0x14
    
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
    UART_CMD_GET_ENV_BRIGHTNESS = 0x42
    
    UART_CMD_SET_LCD_CONTRAST = 0x50
    UART_CMD_GET_LCD_CONTRAST = 0x51
    
    UART_CMD_CREATE_SCROLL_BUFFER = 0x60
    UART_CMD_DELETE_SCROLL_BUFFER = 0x61
    UART_CMD_UPDATE_SCROLL_BUFFER = 0x62
    UART_CMD_SET_DESTINATION_BUFFER = 0x63
    UART_CMD_GET_DESTINATION_BUFFER = 0x64
    UART_CMD_SET_MASK_ENABLED = 0x65
    UART_CMD_GET_MASK_ENABLED = 0x66

    # Raspberry Pi BCM GPIO pin
    # Names as seen from the STM32
    PIN_CTRL_AUX1_OUT = 23
    PIN_CTRL_AUX2_OUT = 24
    PIN_CTRL_AUX1_IN = 17
    PIN_CTRL_AUX2_IN = 27
    
    def __init__(self, uart_port, spi_port, uart_baud = 115200, uart_timeout = 1.0, spi_clock = 5000000, width = 480, height = 128, panel_width = 96, panel_height = 64):
        if not _HAS_SPIDEV:
            raise RuntimeError("spidev module not installed. If you are running this on a PC, use FIAEmulator instead.")
        self.uart = serial.Serial(uart_port, baudrate=uart_baud, timeout=uart_timeout)
        self.spi = spidev.SpiDev()
        self.spi.open(*spi_port)
        self.spi.max_speed_hz = spi_clock
        self.width = width
        self.height = height
        self.panel_width = panel_width
        self.panel_height = panel_height
    
    def exit(self):
        pass
    
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
    
    def set_lcd_contrast(self, side_a, side_b):
        params = [side_a >> 8, side_a & 0xFF, side_b >> 8, side_b & 0xFF]
        resp = self.send_uart_command(self.UART_CMD_SET_LCD_CONTRAST, params)
    
    def get_lcd_contrast(self):
        resp = self.send_uart_command(self.UART_CMD_GET_LCD_CONTRAST)
        side_a = (resp[0] << 8) | resp[1]
        side_b = (resp[2] << 8) | resp[3]
        return side_a, side_b
    
    def create_scroll_buffer(self, side, disp_x, disp_y, disp_w, disp_h, int_w, int_h, sc_off_x, sc_off_y, sc_sp_x, sc_sp_y, sc_st_x, sc_st_y):
        sc_st_x = self.twos_comp(sc_st_x, 16)
        sc_st_y = self.twos_comp(sc_st_y, 16)
        params = [
            side,
            disp_x >> 8, disp_x & 0xFF,
            disp_y >> 8, disp_y & 0xFF,
            disp_w >> 8, disp_w & 0xFF,
            disp_h >> 8, disp_h & 0xFF,
            int_w >> 8, int_w & 0xFF,
            int_h >> 8, int_h & 0xFF,
            sc_off_x >> 8,sc_off_x & 0xFF,
            sc_off_y >> 8,sc_off_y & 0xFF,
            sc_sp_x >> 8,sc_sp_x & 0xFF,
            sc_sp_y >> 8,sc_sp_y & 0xFF,
            sc_st_x >> 8,sc_st_x & 0xFF,
            sc_st_y >> 8,sc_st_y & 0xFF,
        ]
        resp = self.send_uart_command(self.UART_CMD_CREATE_SCROLL_BUFFER, params)
        if resp[0] & self.SCROLL_BUF_ERR_MASK:
            err = resp[0] & ~self.SCROLL_BUF_ERR_MASK
            if err == self.SCROLL_BUF_ERR_COUNT:
                raise FIAError("No free scroll buffer slots left")
            elif err == self.SCROLL_BUF_ERR_SIZE:
                raise FIAError("Not enough memory for requested scroll buffer")
            else:
                raise FIAError("Scroll buffer error code {}".format(err))
        return resp[0]
    
    def delete_scroll_buffer(self, buf_id):
        resp = self.send_uart_command(self.UART_CMD_DELETE_SCROLL_BUFFER, [buf_id])
        return resp[0]
    
    def update_scroll_buffer(self, buf_id, side = 0xFF, disp_x = 0xFFFF, disp_y = 0xFFFF, disp_w = 0xFFFF, disp_h = 0xFFFF, sc_off_x = 0xFFFF, sc_off_y = 0xFFFF, sc_sp_x = 0xFFFF, sc_sp_y = 0xFFFF, sc_st_x = 0x7FFF, sc_st_y = 0x7FFF):
        sc_st_x = self.twos_comp(sc_st_x, 16)
        sc_st_y = self.twos_comp(sc_st_y, 16)
        params = [
            buf_id,
            side,
            disp_x >> 8, disp_x & 0xFF,
            disp_y >> 8, disp_y & 0xFF,
            disp_w >> 8, disp_w & 0xFF,
            disp_h >> 8, disp_h & 0xFF,
            sc_off_x >> 8,sc_off_x & 0xFF,
            sc_off_y >> 8,sc_off_y & 0xFF,
            sc_sp_x >> 8,sc_sp_x & 0xFF,
            sc_sp_y >> 8,sc_sp_y & 0xFF,
            sc_st_x >> 8,sc_st_x & 0xFF,
            sc_st_y >> 8,sc_st_y & 0xFF,
        ]
        resp = self.send_uart_command(self.UART_CMD_UPDATE_SCROLL_BUFFER, params)
        return resp[0]
    
    def set_destination_buffer(self, buf_id):
        resp = self.send_uart_command(self.UART_CMD_SET_DESTINATION_BUFFER, [buf_id])
        return resp[0]
    
    def get_destination_buffer(self):
        resp = self.send_uart_command(self.UART_CMD_GET_DESTINATION_BUFFER)
        return resp[0]
    
    def set_mask_enabled(self, state):
        resp = self.send_uart_command(self.UART_CMD_SET_MASK_ENABLED, [state])
    
    def get_mask_enabled(self):
        resp = self.send_uart_command(self.UART_CMD_GET_MASK_ENABLED)
        return resp[0]
    
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

    def send_image(self, img, auto_fit = True):
        if not isinstance(img, Image.Image):
            img = Image.open(img)
        
        img = img.convert('L')
        
        if auto_fit:
            width, height = img.size
            if width > self.width and height > self.height:
                img = img.crop((0, 0, self.width, self.height))
            else:
                img2 = Image.new('L', (self.width, self.height), 'black')
                img2.paste(img, (0, 0))
                img = img2
        
        array, width, height = self.img_to_array(img)
        self.send_array(array)
    
    def send_gif(self, img, auto_fit = True):
        if not isinstance(img, Image.Image):
            img = Image.open(img)
        
        width, height = img.size
        duration = img.info.get('duration', 1000)
        
        frames = []
        for frame in ImageSequence.Iterator(img):
            if auto_fit:
                if width > self.width and height > self.height:
                    frame = frame.crop((0, 0, self.width, self.height)).convert('L')
                    frames.append(frame)
                else:
                    tmp = Image.new('L', (self.width, self.height), 'black')
                    tmp.paste(frame, (0, 0))
                    frames.append(tmp)
            else:
                temp = Image.new('L', (width, height), 'black')
                temp.paste(frame, (0, 0))
                frames.append(temp)
        
        last_frame_time = 0
        cur_frame = 0
        while True:
            now = time.time()
            if now - last_frame_time >= duration / 1000:
                self.send_image(frames[cur_frame], auto_fit)
                last_frame_time = now
                cur_frame += 1
            if cur_frame >= len(frames):
                cur_frame = 0


class FIAEmulator(FIA):
    def __init__(self, width = 480, height = 128, panel_width = 96, panel_height = 64, h_sep_height = 15, v_sep_width = 1, off_colour = (0, 0, 200), on_colour = (255, 255, 255), frame_colour = (0, 0, 50)):
        self.width = width
        self.height = height
        self.panel_width = panel_width
        self.panel_height = panel_height
        self.h_sep_height = h_sep_height
        self.v_sep_width = v_sep_width
        self.off_colour = off_colour
        self.on_colour = on_colour
        self.frame_colour = frame_colour
        
        self.backlight_on = True
        self.h_panels = width // panel_width
        self.v_panels = height // panel_height
        self.img_width = width + (self.h_panels - 1) * v_sep_width
        self.img_height = height + (self.v_panels - 1) * h_sep_height
        self.img = Image.new('L', (self.width, self.height), 'black')
        self.disp_img = Image.new('RGB', (self.img_width, self.img_height), self.frame_colour)
        self.tk_running = True
        self.tk_img_updated = True
        self.tk_thread = threading.Thread(target=self.tk_loop)
        self.tk_thread.start()
    
    def exit(self):
        self.tk_running = False
        self.tk_thread.join()
    
    def _replace_colour(self, img, src, repl):
        data = np.array(img)
        r1, g1, b1 = src
        r2, g2, b2 = repl
        red, green, blue = data[:,:,0], data[:,:,1], data[:,:,2]
        mask = (red == r1) & (green == g1) & (blue == b1)
        data[:,:,:3][mask] = [r2, g2, b2]
        return Image.fromarray(data)

    def _make_disp_img(self, img):
        if self.backlight_on:
            on_colour = self.on_colour
            off_colour = self.off_colour
        else:
            on_colour = tuple([0.2 * v for v in self.on_colour])
            off_colour = tuple([0.2 * v for v in self.off_colour])
        
        ret = Image.new('RGB', (self.img_width, self.img_height), self.frame_colour)
        for y_panel in range(self.v_panels):
            for x_panel in range(self.h_panels):
                src_x1 = x_panel * self.panel_width
                src_y1 = y_panel * self.panel_height
                src_x2 = src_x1 + self.panel_width
                src_y2 = src_y1 + self.panel_height
                dest_x1 = x_panel * (self.panel_width + self.v_sep_width)
                dest_y1 = y_panel * (self.panel_height + self.h_sep_height)
                panel = img.crop((src_x1, src_y1, src_x2, src_y2)).convert("RGB")
                if off_colour != (0, 0, 0):
                    panel = self._replace_colour(panel, (0, 0, 0), off_colour)
                if on_colour != (255, 255, 255):
                    panel = self._replace_colour(panel, (255, 255, 255), on_colour)
                ret.paste(panel, (dest_x1, dest_y1))
        return ret
    
    def tk_update(self):
        self.disp_img = self._make_disp_img(self.img)
        self.tk_img_updated = True
    
    def tk_loop(self):
        def _on_close():
            self.tk_running = False
        
        window = tk.Tk()
        window.protocol("WM_DELETE_WINDOW", _on_close)
        window.title("FIA Emulator")
        window.call('wm', 'attributes', '.', '-topmost', '1')
        window.resizable(False, False)
        canvas = tk.Canvas(window, width=self.img_width, height=self.img_height)
        canvas.pack()
        tk_img = ImageTk.PhotoImage(self.disp_img)
        canvas_img = canvas.create_image(0, 0, anchor='nw', image=tk_img)
        while self.tk_running:
            if self.tk_img_updated:
                tk_img = ImageTk.PhotoImage(self.disp_img)
                canvas.itemconfig(canvas_img, image=tk_img)
                self.tk_img_updated = False
            window.update_idletasks()
            window.update()
        window.destroy()
    
    def send_uart_command_raw(self, raw_command):
        print("TX: " + raw_command)
    
    def read_uart_response(self):
        return bytearray([0xFF] * 100)
    
    def set_backlight_state(self, state):
        super().set_backlight_state(state)
        self.backlight_on = state

    def send_image(self, img, auto_fit = True):
        if not isinstance(img, Image.Image):
            img = Image.open(img)
        
        img = img.convert('L')
        
        if auto_fit:
            width, height = img.size
            if width > self.width and height > self.height:
                img = img.crop((0, 0, self.width, self.height))
            else:
                img2 = Image.new('L', (self.width, self.height), 'black')
                img2.paste(img, (0, 0))
                img = img2
        
        fn = lambda x : 255 if x > 64 else 0
        self.img = img.convert('L').point(fn, mode='1')
        self.tk_update()