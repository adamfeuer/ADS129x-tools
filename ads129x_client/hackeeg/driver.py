import json
import sys
import time
from json import JSONDecodeError

import serial
import msgpack

from . import ads1299

# TODO
# - MessagePack
# - MessagePack / Json Lines testing convenience functions

NUMBER_OF_SAMPLES = 10000
DEFAULT_BAUDRATE = 115200
SAMPLE_LENGTH_IN_BYTES = 38  # 216 bits encoded with base64 + '\r\n\'


class Status:
    Ok = 200
    BadRequest = 400
    Error = 500


SPEEDS = {250: ads1299.HIGH_RES_250_SPS,
          500: ads1299.HIGH_RES_500_SPS,
          1024: ads1299.HIGH_RES_1k_SPS,
          2048: ads1299.HIGH_RES_2k_SPS,
          4096: ads1299.HIGH_RES_4k_SPS,
          8192: ads1299.HIGH_RES_8k_SPS,
          16384: ads1299.HIGH_RES_16k_SPS}


class HackEEGException(Exception):
    pass


class HackEEGBoard:
    TextMode = 0
    JsonLinesMode = 1
    MessagePackMode = 2

    CommandKey = "COMMAND"
    ParametersKey = "PARAMETERS"
    HeadersKey = "HEADERS"
    DataKey = "DATA"
    DecodedDataKey = "DECODED_DATA"
    StatusCodeKey = "STATUS_CODE"
    StatusTextKey = "STATUS_TEXT"

    MpCommandKey = "C"
    MpParametersKey = "P"
    MpHeadersKey = "H"
    MpDataKey = "D"
    MpStatusCodeKey = "C"
    MpStatusTextKey = "T"
    MaxConnectionAttempts = 10
    ConnectionSleepTime = 0.1

    def __init__(self, serialPortPath=None, baudrate=DEFAULT_BAUDRATE, debug=False):
        self.mode = None
        self.message_pack_unpacker = None
        self.debug = debug
        self.baudrate = baudrate
        self.rdatac_mode = False
        self.serialPortPath = serialPortPath
        if serialPortPath:
            self.serialPort = serial.serial_for_url(serialPortPath, baudrate=self.baudrate, timeout=0.1)
            self.message_pack_unpacker = msgpack.Unpacker(self.serialPort, raw=False)
            self.serialPort.reset_input_buffer()

    def connect(self):
        self.mode = self._sense_protocol_mode()
        if self.mode == self.TextMode:
            attempts = 0
            connected = False
            while attempts < self.MaxConnectionAttempts:
                try:
                    self.jsonlines_mode()
                    connected = True
                    break
                except JSONDecodeError:
                    if attempts == 0:
                        print("Connecting...", end='')
                    elif attempts > 0:
                        print('.', end='')
                    sys.stdout.flush()
                    attempts += 1
                    time.sleep(self.ConnectionSleepTime)
            if attempts > 0:
                print()
            if not connected:
                raise HackEEGException("Can't connect to Arduino")
        self.sdatac()

    def _serial_write(self, command):
        command_data = bytes(command, 'utf-8')
        self.serialPort.write(command_data)

    def _serial_readline(self):
        line = None
        while not line:
            try:
                line = self.serialPort.readline()
                line = line.decode("utf-8")
            except UnicodeDecodeError:
                if self.debug:
                    print("UnicodeDecodeError")
        line = line.strip()
        if self.debug:
            print(f"line: {line}")
        return line

    def _serial_read_messagepack_message(self):
        message = self.message_pack_unpacker.unpack()
        if self.debug:
            print(f"message: {message}")
        return message

    def _decode_data(self, response):
        """decode ADS1299 sample status bits - datasheet, p36
        The format is:
        1100 + LOFF_STATP[0:7] + LOFF_STATN[0:7] + bits[4:7] of the GPIOregister"""
        if response:
            data = response.get(self.DataKey)
            if data and type(data) is list:
                timestamp = int.from_bytes(data[0:4], byteorder='little')
                ads_status = int.from_bytes(data[4:7], byteorder='big')
                ads_gpio = ads_status & 0x0f
                loff_statn = (ads_status >> 4) & 0xff
                loff_statp = (ads_status >> 12) & 0xff
                extra = (ads_status >> 20) & 0xff

                channel_data = []
                for channel in range(0, 8):
                    channel_offset = 7 + (channel * 3)
                    sample = int.from_bytes(data[channel_offset:channel_offset + 3], byteorder='little')
                    channel_data.append(sample)

                decoded_data = dict(timestamp=timestamp, ads_status=ads_status, ads_gpio=ads_gpio, loff_statn=loff_statn, loff_statp=loff_statp, extra=extra, channel_data=channel_data)
                response[self.DecodedDataKey] = decoded_data
        return response

    def set_debug(self, debug):
        self.debug = debug

    def read_response(self):
        """read a response from the Arduino– must be in JSON Lines mode"""
        message = self._serial_readline()
        try:
            response_obj = json.loads(message)
        except UnicodeDecodeError:
            response_obj = None
        if self.debug:
            print(f"read_response line: {message}")
        if self.debug:
            print("json response:")
            print(self.format_json(response_obj))
        return self._decode_data(response_obj)

    def read_rdatac_response(self):
        """read a response from the Arduino– JSON Lines or MessagePack mode are ok"""
        if self.mode == self.MessagePackMode:
            response_obj = self._serial_read_messagepack_message()
        else:
            message = self._serial_readline()
            response_obj = json.loads(message)
        if self.debug:
            print(f"read_response obj: {response_obj}")
        result = None
        try:
            result = self._decode_data(response_obj)
        except AttributeError:
            pass
        return result

    def format_json(self, json_obj):
        return json.dumps(json_obj, indent=4, sort_keys=True)

    def send_command(self, command, parameters=None):
        if self.debug:
            print(f"command: {command}  parameters: {parameters}")
        if self.mode in (self.JsonLinesMode, self.MessagePackMode):
            # commands are only sent in JSON Lines mode
            new_command_obj = {self.CommandKey: command, self.ParametersKey: parameters}
            new_command = json.dumps(new_command_obj)
        else:
            raise HackEEGException("Unknown mode")
        if self.debug:
            print("json command:")
            print(self.format_json(new_command_obj))
        self._serial_write(new_command)
        self._serial_write('\n')

    def send_text_command(self, command):
        self._serial_write(command + '\n')

    def execute_command(self, command, parameters=None):
        if parameters is None:
            parameters = []
        self.send_command(command, parameters)
        response = self.read_response()
        return response

    def _sense_protocol_mode(self):
        try:
            result = self.execute_command("nop")
            return self.JsonLinesMode
        except Exception:
            return self.TextMode

    def ok(self, response):
        return response.get(self.StatusCodeKey) == Status.Ok

    def wreg(self, register, value):
        command = "wreg"
        parameters = [register, value]
        return self.execute_command(command, parameters)

    def rreg(self, register):
        command = "rreg"
        parameters = [register]
        response = self.execute_command(command, parameters)
        return response

    def nop(self):
        return self.execute_command("nop")

    def boardledon(self):
        return self.execute_command("boardledon")

    def boardledoff(self):
        return self.execute_command("boardledoff")

    def ledon(self):
        return self.execute_command("ledon")

    def ledoff(self):
        return self.execute_command("ledoff")

    def micros(self):
        return self.execute_command("micros")

    def text_mode(self):
        return self.send_command("text")

    def reset(self):
        return self.execute_command("reset")

    def start(self):
        return self.execute_command("start")

    def stop(self):
        return self.execute_command("stop")

    def status(self):
        return self.execute_command("status")

    def jsonlines_mode(self):
        old_mode = self.mode
        self.mode = self.JsonLinesMode
        if old_mode == self.TextMode:
            self.send_text_command("jsonlines")
            return self.read_response()
        if old_mode == self.JsonLinesMode:
            self.execute_command("jsonlines")

    def messagepack_mode(self):
        old_mode = self.mode
        self.mode = self.MessagePackMode
        if old_mode == self.TextMode:
            self.send_text_command("messagepack")
            response = self._serial_read_messagepack_message()
            return response
        elif old_mode == self.JsonLinesMode:
            self.send_command("messagepack")
            response = self._serial_read_messagepack_message()
            return response

    def rdatac(self):
        result = self.execute_command("rdatac")
        if self.ok(result):
            self.rdatac_mode = True
        return result

    def sdatac(self):
        result = self.execute_command("sdatac")
        self.rdatac_mode = False
        return result

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
        command = "wreg"
        parameters = [ads1299.CHnSET + channel, ads1299.PDn]
        self.execute_command(command, parameters)

    def enable_all_channels(self):
        for channel in range(1, 9):
            self.enable_channel(channel)

    def disable_all_channels(self):
        for channel in range(1, 9):
            self.disable_channel(channel)

    def blink_board_led(self):
        self.execute_command("boardledon")
        time.sleep(0.3)
        self.execute_command("boardledoff")
