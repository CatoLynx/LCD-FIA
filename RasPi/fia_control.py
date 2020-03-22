import serial
import spidev

class FIA:
    UART_CMD_NULL = 0x00
    
    UART_CMD_SET_BACKLIGHT_STATE = 0x10
    UART_CMD_GET_BACKLIGHT_STATE = 0x11
    UART_CMD_SET_BACKLIGHT_BASE_BRIGHTNESS = 0x12
    UART_CMD_GET_BACKLIGHT_BASE_BRIGHTNESS = 0x13
    
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
    
    def __init__(self, uart_port, spi_port, uart_baud = 115200, uart_timeout = 1.0, spi_clock = 5000000):
        self.uart = serial.Serial(uart_port, baudrate=uart_baud, timeout=uart_timeout)
        self.spi = spidev.SpiDev()
        self.spi.open(*spi_port)
        self.spi.max_speed_hz = spi_clock
    
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
    
    def send_uart_command(self, command, data = []):
        # Send UART command and return response
        assert len(data) <= 254
        raw_command = [0xFF, len(data) + 2, command]
        raw_command += data
        checksum = self.calculate_uart_checksum([command] + data)
        raw_command.append(checksum)
        self.send_uart_command_raw(raw_command)
        return self.read_uart_response()
    
    def null_cmd(self):
        resp = self.send_uart_command(self.UART_CMD_NULL)
    
    def set_backlight_state(self, state):
        resp = self.send_uart_command(self.UART_CMD_SET_BACKLIGHT_STATE, [state])
    
    def get_backlight_state(self):
        resp = self.send_uart_command(self.UART_CMD_GET_BACKLIGHT_STATE)
        return resp[0]
    
    def set_backlight_base_brightness(self, side_a, side_b):
        params = [side_a & 0xFF, side_a >> 8, side_b & 0xFF, side_b >> 8]
        resp = self.send_uart_command(self.UART_CMD_SET_BACKLIGHT_BASE_BRIGHTNESS, params)
    
    def get_backlight_base_brightness(self):
        resp = self.send_uart_command(self.UART_CMD_GET_BACKLIGHT_BASE_BRIGHTNESS)
        side_a = (resp[1] << 8) | resp[0]
        side_b = (resp[3] << 8) | resp[2]
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
        return t_ext1, t_ext2
