import base64
import sys
import time

import bitstring
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

SPEEDS = {250: ads1299.HIGH_RES_250_SPS,
          500: ads1299.HIGH_RES_500_SPS,
          1024: ads1299.HIGH_RES_1k_SPS,
          2048: ads1299.HIGH_RES_2k_SPS,
          4096: ads1299.HIGH_RES_4k_SPS,
          8192: ads1299.HIGH_RES_8k_SPS,
          16384: ads1299.HIGH_RES_16k_SPS}


class HackEegException(Exception):
    pass


def BitSequence(name, *subcons):
    r"""A struct of bitwise fields

    :param name: the name of the struct
    :param \*subcons: the subcons that make up this structure
    """
    return construct.Bitwise(construct.Sequence(name, *subcons))


class HackEegBoard():
    def __init__(self, serialPortPath=None, baudrate=DEFAULT_BAUDRATE):
        if serialPortPath is None:
            raise HackEegException("You must specify a serial port!")
        self.serialPortPath = serialPortPath
        self.serialPort = serial.serial_for_url(
            serialPortPath, baudrate=baudrate, timeout=0.1)
        print(f"using device: {self.serialPort.portstr}")
        # self.sampleParser = BitSequence("sample",
        #                                 construct.BitField("start", 4),
        #                                 construct.BitField("loff_statp", 8),
        #                                 construct.BitField("loff_statn", 8),
        #                                 construct.BitField("gpio", 4),
        #                                 construct.BitField("channel1", 24),
        #                                 construct.BitField("channel2", 24),
        #                                 construct.BitField("channel3", 24),
        #                                 construct.BitField("channel4", 24),
        #                                 construct.BitField("channel5", 24),
        #                                 construct.BitField("channel6", 24),
        #                                 construct.BitField("channel7", 24),
        #                                 construct.BitField("channel8", 24))

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

    def executeCommand(self, command, parameters):
        self.sendCommand(command, parameters)
        response = self.readResponse()
        return response

    def wreg(self, register, value):
        command = "wreg %02x %02x" % (register, value)
        print(f"command: {command}")
        self.executeCommand(command)

    def rreg(self, register):
        command = "rreg %02x" % register
        print(f"command: {command}")
        self.executeCommand(command)
        line = self.readUntilNonBlankLineCountdown()
        print(line)

    def rdatac(self):
        self.executeCommand("rdatac")

    def sdatac(self):
        self.executeCommand("sdatac")

    def start(self):
        self.executeCommand("start")

    def stop(self):
        self.executeCommand("stop")

    def enableChannel(self, channel):
        self.sdatac()
        command = "wreg %02x %02x" % (
            ads1299.CHnSET + channel, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        self.executeCommand(command)
        self.rdatac()

    def disableChannel(self, channel):
        self.sdatac()
        self._disableChannel(channel)
        self.rdatac()

    def _disableChannel(self, channel):
        command = "wreg %02x %02x" % (ads1299.CHnSET + channel, ads1299.PDn)
        self.executeCommand(command)


    def readSampleBytes(self):
        line = self.serialPort.readline()
        while len(line) != SAMPLE_LENGTH_IN_BYTES:  # \r\n
            line = self.serialPort.readline()
            print(line)
            print(".", end="")
            sys.stdout.flush()
        decodedSampleBytes = base64.b64decode(line[:-1])
        # print("driver, decoded len: {}".format(len(decodedSampleBytes)))
        # print(' '.join(x.encode('hex') for x in decodedSampleBytes))
        return decodedSampleBytes

    def testBit(int_type, offset):
        mask = 1 << offset
        return(int_type & mask)

    def readSample(self):
        sample = self.readSampleBytes()
        channelData = [0, 0, 0, 0]
        for i in range(0, 8):
            datapoint = sample[3 + i * 3:3 + i * 3 + 3]
            if(ord(datapoint[0]) & 0x80):
                newDatapoint = chr(0xFF) + datapoint
            else:
                newDatapoint = chr(0x00) + datapoint
            datapointBitstring = bitstring.BitArray(bytes=newDatapoint)
            unpackedSample = datapointBitstring.unpack("int:32")
            channelData += unpackedSample
        data = np.array(channelData) / (2. ** (24 - 1))
        print("channel data: {} {}".format(
            channelData[4 + 4], channelData[4 + 6]))
        return channelData

    def readSample_unused(self):
        #result = self.sampleParser.parse(self.readSampleBytes())
        sample = bitstring.BitArray(bytes=self.readSampleBytes())
        result = sample.unpack(
            "uint:4, uint:8, uint:8, uint:4, int:24, int:24, int:24, int:24, int:24, int:24, int:24, int:24")
        return result

    def blinkBoardLed(self):
        self.executeCommand("boardledon")
        time.sleep(0.3)
        self.executeCommand("boardledoff")
