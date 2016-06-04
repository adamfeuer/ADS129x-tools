import traceback
import sys, time, datetime, base64
import serial, bitstring
import numpy as np
#import serial, construct
import ads1299

NUMBER_OF_SAMPLES = 10000
DEFAULT_BAUDRATE = 115200
SAMPLE_LENGTH_IN_BYTES = 38 # 216 bits encoded with base64 + '\r\n\'

SPEEDS = {   250 : ads1299.HIGH_RES_250_SPS,
             500 : ads1299.HIGH_RES_500_SPS,
            1024 : ads1299.HIGH_RES_1k_SPS,
            2048 : ads1299.HIGH_RES_2k_SPS,
            4096 : ads1299.HIGH_RES_4k_SPS,
            8192 : ads1299.HIGH_RES_8k_SPS,
           16384 : ads1299.HIGH_RES_16k_SPS }

class HackEegException(Exception):
   pass


def BitSequence(name, *subcons):
    r"""A struct of bitwise fields

    :param name: the name of the struct
    :param \*subcons: the subcons that make up this structure
    """
    return construct.Bitwise(construct.Sequence(name, *subcons))


class HackEegBoard():
   def __init__(self, serialPortPath = None, baudrate = DEFAULT_BAUDRATE):
      if serialPortPath is None:
         raise HackEegException, "You must specify a serial port!"
      self.serialPortPath = serialPortPath
      self.serialPort = serial.serial_for_url(serialPortPath, baudrate=baudrate, timeout=0.1)
      print("using device: %s" % self.serialPort.portstr)
      #self.sampleParser = BitSequence("sample",
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


   def foundStatusLine(self, lineToTest):
      line = lineToTest.strip()
      if len(line) == 0:
         #print "line len = 0"
         return False
      try:
         #print "about to split"
         tokens = line.split()
         #print line
         #print tokens
         returnCode = tokens[0]
         if int(returnCode) == 200:
            #print "**** found 200 Ok!"
            line = self.serialPort.readline()
            #print line
            return True
      except:
         #tb = traceback.format_exc()
         #print tb
         return False

   def readUntilStatusLine(self):
      line = self.readUntilNonBlankLine()
      while self.foundStatusLine(line) is False:
         print "rusl.. %s" % line
         line = self.serialPort.readline()
      print "rusl-.. %s" % line

   def readUntilNonBlankLineCountdown(self, count=20):
      countDown = count
      line = self.serialPort.readline()
      line = line.strip()
      while len(line) == 0 and countDown > 0:
         #print "runbcd.. %s" % line
         line = self.serialPort.readline()
         countDown = countDown - 1
      #print "runbcd.. %s" % line
      return line

   def readUntilNonBlankLine(self):
      line = self.serialPort.readline()
      line = line.strip()
      while len(line) == 0:
         #print "runb.. %s" % line
         line = self.serialPort.readline()
      #print "runb.. %s" % line
      return line

   def wreg(self, register, value):
      command = "wreg %02x %02x" % (register, value)
      print "command: %s" % command
      self.serialPort.write(bytes(command +'\n'))
      self.readUntilStatusLine()

   def rreg(self, register):
      command = "rreg %02x" % register
      print "command: %s" % command
      self.serialPort.write(bytes(command +'\n'))
      self.readUntilStatusLine()
      line = self.readUntilNonBlankLineCountdown()
      print line

   def send(self, command):
      print "command: %s" % command
      self.serialPort.write(bytes(command +'\n'))
      self.readUntilStatusLine()
      #self.readUntilNonBlankLineCountdown()

   def sendAsync(self, command):
      print "(async) command: %s" % command
      self.serialPort.write(bytes(command +'\n'))

   def rdatac(self):
      #python3.x
      #self.serialPort.write(bytes('rdatac\n', 'utf-8'))
      self.serialPort.write(bytes('rdatac\n'))
      line = self.serialPort.readline()
      print line
      line = self.serialPort.readline()
      print line

   def sdatac(self):
      #self.sendAsync("sdatac")
      #self.sendAsync("sdatac")
      self.send("sdatac")

   def enableChannel(self, channel):
      self.send("sdatac")
      command = "wreg %02x %02x" % (ads1299.CHnSET + channel, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
      self.send(command)
      self.rdatac()

   def disableChannel(self, channel):
      self.send("sdatac")
      self._disableChannel(channel)
      self.rdatac()

   def _disableChannel(self, channel):
      command = "wreg %02x %02x" % (ads1299.CHnSET + channel, ads1299.PDn)
      self.send(command)

   def setup(self, samplesPerSecond = 500):
      if samplesPerSecond not in SPEEDS.keys():
         raise HackEegException, "%s is not a valid speed; valid speeds are %s" % (samplesPerSecond, sorted(SPEEDS.keys()))
      self.sdatac()
      time.sleep(1)
      #self.send("reset")
      self.blinkBoardLed()
      self.send("base64")
      sampleMode = ads1299.HIGH_RES_250_SPS | ads1299.CONFIG1_const
      sampleModeCommand = "wreg %x %x" % (ads1299.CONFIG1, sampleMode)
      self.send(sampleModeCommand)
      self._disableChannel(1)
      self._disableChannel(2)
      self._disableChannel(3)
      self._disableChannel(4)
      #self._disableChannel(5)
      self._disableChannel(6)
      #self._disableChannel(7)
      self._disableChannel(8)
      self.wreg(ads1299.CH5SET, ads1299.TEST_SIGNAL | ads1299.GAIN_2X)
      #self.wreg(ads1299.CH7SET, ads1299.TEMP | ads1299.GAIN_12X)
      self.rreg(ads1299.CH5SET)
      #self.wreg(ads1299.CH8SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
      self.wreg(ads1299.CH7SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_2X)
      #self.wreg(ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
      self.rreg(ads1299.CH8SET)
      #self.rreg(ads1299.CH2SET)
      #command = "wreg %x %x" % (ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
      #self.send(command)
      #command = "wreg %x %x" % (ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
      #self.send(command)
      # Unipolar mode - setting SRB1 bit sends mid-supply voltage to the N inputs
      self.wreg(ads1299.MISC1, ads1299.SRB1)
      # add channels into bias generation
      self.wreg(ads1299.BIAS_SENSP, ads1299.BIAS8P)
      self.rdatac()
      return

   def readSampleBytes(self):
      line = self.serialPort.readline()
      while len(line) != SAMPLE_LENGTH_IN_BYTES: # \r\n
         line = self.serialPort.readline()
         print ".",
         sys.stdout.flush()
      decodedSampleBytes = base64.b64decode(line[:-1])
      # print "driver, decoded len: %d" % len(decodedSampleBytes)
      # print ' '.join(x.encode('hex') for x in decodedSampleBytes)
      return decodedSampleBytes

   def testBit(int_type, offset):
      mask = 1 << offset
      return(int_type & mask)

   def readSample(self):
      sample = self.readSampleBytes()
      #print ' '.join(x.encode('hex') for x in sample)
      channelData = [0,0,0,0]
      for i in range(0, 8):
         datapoint = sample[3+i*3:3+i*3+3]
         #print ' '.join(x.encode('hex') for x in datapoint)
         if(ord(datapoint[0]) & 0x80):
            newDatapoint = chr(0xFF) + datapoint
         else:
            newDatapoint = chr(0x00) + datapoint
         #print ' '.join(x.encode('hex') for x in newDatapoint)
         datapointBitstring = bitstring.BitArray(bytes=newDatapoint)
         unpackedSample = datapointBitstring.unpack("int:32")
         channelData += unpackedSample
      data = np.array(channelData) / (2. ** (24 - 1));
      print("channel data: {} {}".format(channelData[4+4],channelData[4+6]))
      return channelData

   def readSample_unused(self):
      #result = self.sampleParser.parse(self.readSampleBytes())
      sample = bitstring.BitArray(bytes=self.readSampleBytes())
      result = sample.unpack("uint:4, uint:8, uint:8, uint:4, int:24, int:24, int:24, int:24, int:24, int:24, int:24, int:24")
      #result = sample.unpack("uint:4, uintle:8, uintle:8, uint:4, intle:24, intle:24, intle:24, intle:24, intle:24, intle:24, intle:24, intle:24")
      #print result
      return result


   def blinkBoardLed(self):
     self.send("boardledon")
     time.sleep(0.3)
     self.send("boardledoff")
