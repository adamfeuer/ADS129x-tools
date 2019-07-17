import json
import time

import serial

from hackeeg import ads1299

# TODO
# - MessagePack

NUMBER_OF_SAMPLES = 10000
DEFAULT_BAUDRATE = 115200
SAMPLE_LENGTH_IN_BYTES = 38  # 216 bits encoded with base64 + '\r\n\'

COMMAND = "COMMAND"
PARAMETERS = "PARAMETERS"
HEADERS = "HEADERS"
DATA = "DATA"


class Status:
    OK = 200


class Mode:
    TEXT = 0
    JSONLINES = 1
    MESSAGEPACK = 2


SPEEDS = {250: ads1299.HIGH_RES_250_SPS,
          500: ads1299.HIGH_RES_500_SPS,
          1024: ads1299.HIGH_RES_1k_SPS,
          2048: ads1299.HIGH_RES_2k_SPS,
          4096: ads1299.HIGH_RES_4k_SPS,
          8192: ads1299.HIGH_RES_8k_SPS,
          16384: ads1299.HIGH_RES_16k_SPS}


class HackEegException(Exception):
    pass


class HackEegBoard():
    def __init__(self, serialPortPath=None, baudrate=DEFAULT_BAUDRATE):
        self.rdatac_mode = False
        self.protocol_mode = Mode.TEXT
        self.serialPortPath = serialPortPath
        if serialPortPath:
            self.serialPort = serial.serial_for_url(serialPortPath, baudrate=baudrate, timeout=0.1)
            self.jsonlines_mode()

    def _serial_write(self, command):
        command_data = bytes(command, 'utf-8')
        self.serialPort.write(command_data)

    def _serial_readline(self):
        line = self.serialPort.readline()
        line = line.decode("utf-8")
        line = line.strip()
        print(f"line: {line}")
        return line

    def read_response(self):
        line = self._serial_readline()
        # TODO remove comment ignorer
        while line.startswith("#"):
            line = self._serial_readline()
        response_obj = json.loads(line)
        print("json response:")
        print(self.format_json(response_obj))
        return response_obj

    def format_json(self, json_obj):
        return json.dumps(json_obj, indent=4, sort_keys=True)

    def send_command(self, command, parameters):
        print(f"command: {command}  parameters: {parameters}")
        new_command_obj = dict(COMMAND=command, PARAMETERS=parameters)
        new_command = json.dumps(new_command_obj)
        print("json command:")
        print(self.format_json(new_command_obj))
        self._serial_write(new_command + '\n')

    def send_text_command(self, command):
        self._serial_write(command + '\n')

    def execute_command(self, command, parameters=None):
        if parameters is None:
            parameters = []
        self.send_command(command, parameters)
        response = self.read_response()
        return response

    def ok(self, response):
        return response[COMMAND] == Status.OK

    def wreg(self, register, value):
        command = "wreg"
        parameters = [register, value]
        self.execute_command(command, parameters)

    def rreg(self, register):
        command = "rreg"
        parameters = [register]
        self.execute_command(command)
        response = self.read_response()
        return response

    def text_mode(self):
        self.execute_command("text")
        response = self.read_response()
        return response

    def jsonlines_mode(self):
        self.send_text_command("jsonlines")
        self.protocol_mode = Mode.JSONLINES
        response = self.read_response()
        return response

    def messagepack_mode(self):
        return self.send_text_command("messagepack")

    def rdatac(self):
        result = self.execute_command("rdatac")
        if self.ok(result):
            self.rdatac_mode = True
        return result

    def sdatac(self):
        self.execute_command("sdatac")
        self.rdatac_mode = False

    def start(self):
        self.execute_command("start")

    def stop(self):
        self.execute_command("stop")

    def enable_channel(self, channel, gain=None):
        if gain is None:
            gain = ads1299.GAIN_1X
        temp_rdatac_mode = self.rdatac_mode
        if self.rdatac_mode:
            self.sdatac()
        command = "wreg"
        parameters = [ads1299.CHnSET + channel, ads1299.ELECTRODE_INPUT | gain]
        self.execute_command(command, parameters)
        if temp_rdatac_mode:
            self.rdatac()

    def disable_channel(self, channel):
        self._disable_channel(channel)

    def _disable_channel(self, channel):
        command = "wreg"
        parameters = [ads1299.CHnSET + channel, ads1299.PDn]
        self.execute_command(command, parameters)

    def blink_board_led(self):
        self.execute_command("boardledon")
        time.sleep(0.3)
        self.execute_command("boardledoff")
