import sys
import time

import numpy as np
import serial
import json

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
        self.mode = Mode.TEXT
        self.serialPortPath = serialPortPath
        if serialPortPath:
            self.serialPort = serial.serial_for_url(serialPortPath, baudrate=baudrate, timeout=0.1)
            self.jsonlines_mode()

    def _serialWrite(self, command):
        command_data = bytes(command, 'utf-8')
        self.serialPort.write(command_data)

    def readResponse(self):
        line = self.serialPort.readline()
        line = line.strip()
        response_obj = json.loads(line)
        print("json response:")
        print(self.format(response_obj))
        return response_obj

    def format_json(self, json_obj):
        return json.dumps(json_obj, indent=4, sort_keys=True)

    def sendCommand(self, command, parameters):
        print(f"command: {command}  parameters: {parameters}")
        new_command_obj = dict(COMMAND=command, PARAMETERS=parameters)
        new_command = json.dumps(new_command_obj)
        print("json command:")
        print(self.format(new_command_obj))
        self._serialWrite(new_command + '\n')

    def sendTextCommand(self, command):
        self._serialWrite(command + '\n')

    def executeCommand(self, command, parameters):
        self.sendCommand(command, parameters)
        response = self.readResponse()
        return response

    def ok(self, response):
        return response[COMMAND] == Status.OK

    def wreg(self, register, value):
        command = "wreg"
        parameters = [register, value]
        self.executeCommand(command, parameters)

    def rreg(self, register):
        command = "rreg"
        parameters = [register]
        self.executeCommand(command)
        response = self.readResponse()
        return response

    def text_mode(self):
        self.executeCommand("text")
        response = self.readResponse()
        return response

    def jsonlines_mode(self):
        return self.sendTextCommand("jsonlines")

    def messagepack_mode(self):
        return self.sendTextCommand("messagepack")

    def rdatac(self):
        result = self.executeCommand("rdatac")
        if self.ok(result):
            self.rdatac_mode = True
        return result

    def sdatac(self):
        self.executeCommand("sdatac")
        self.rdatac_mode = False

    def start(self):
        self.executeCommand("start")

    def stop(self):
        self.executeCommand("stop")

    def enableChannel(self, channel, gain=None):
        if gain is None:
            gain = ads1299.GAIN_1X
        temp_rdatac_mode = self.rdatac_mode
        if self.rdatac_mode:
            self.sdatac()
        command = "wreg"
        parameters = [ads1299.CHnSET + channel, ads1299.ELECTRODE_INPUT | gain]
        self.executeCommand(command, parameters)
        if temp_rdatac_mode:
            self.rdatac()

    def disableChannel(self, channel):
        self._disableChannel(channel)

    def _disableChannel(self, channel):
        command = "wreg"
        parameters = [ads1299.CHnSET + channel, ads1299.PDn]
        self.executeCommand(command, parameters)

    def blinkBoardLed(self):
        self.executeCommand("boardledon")
        time.sleep(0.3)
        self.executeCommand("boardledoff")
