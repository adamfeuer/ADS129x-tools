#!/usr/bin/env python

import sys
import time

import hackeeg
from hackeeg import ads1299
from hackeeg.driver import SPEEDS


# TODO
# - argparse commandline arguments
# - JSONLines, MessagePack


class HackEegTestApplication:
    def __init__(self):
        self.serialPortName = None
        self.hackeeg = None

    def setup(self, samples_per_second=500):
        if samples_per_second not in SPEEDS.keys():
            raise HackEegException("{} is not a valid speed; valid speeds are {}".format(
                samples_per_second, sorted(SPEEDS.keys())))
        sample_mode = ads1299.HIGH_RES_250_SPS | ads1299.CONFIG1_const
        self.hackeeg.wreg(ads1299.CONFIG1, sample_mode)
        self.hackeeg.disable_channel(1)
        self.hackeeg.disable_channel(2)
        self.hackeeg.disable_channel(3)
        self.hackeeg.disable_channel(4)
        # self.hackeeg.disableChannel(5)
        self.hackeeg.disable_channel(6)
        # self.hackeeg.disableChannel(7)
        self.hackeeg.disable_channel(8)
        self.hackeeg.wreg(ads1299.CH5SET, ads1299.TEST_SIGNAL | ads1299.GAIN_2X)
        # self.hackeeg.wreg(ads1299.CH7SET, ads1299.TEMP | ads1299.GAIN_12X)
        self.hackeeg.rreg(ads1299.CH5SET)
        # self.hackeeg.wreg(ads1299.CH8SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        self.hackeeg.wreg(ads1299.CH7SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_1X)
        # self.hackeeg.wreg(ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        # self.hackeeg.rreg(ads1299.CH8SET)
        # self.hackeeg.rreg(ads1299.CH2SET)
        # command = "wreg %x %x" % (ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        # self.hackeeg.send(command)
        # command = "wreg %x %x" % (ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        # self.hackeeg.send(command)
        # Unipolar mode - setting SRB1 bit sends mid-supply voltage to the N
        # inputs
        self.hackeeg.wreg(ads1299.MISC1, ads1299.SRB1)
        # add channels into bias generation
        self.hackeeg.wreg(ads1299.BIAS_SENSP, ads1299.BIAS8P)
        self.hackeeg.rdatac()
        self.hackeeg.start()
        return

    def _unused_setup(self):
        self.hackeeg.jsonlines_mode()
        # self.hackeeg.sdatac()
        time.sleep(1)
        # self.hackeeg.send("reset")
        self.hackeeg.blink_board_led()
        return

    def main(self):
        self.serialPortName = sys.argv[1]
        self.hackeeg = hackeeg.HackEEGBoard(self.serialPortName)
        self.setup()
        self.hackeeg.text_mode()
        # while True:
        #     sample = self.hackeeg.read_sample()
        #     print("{sample}")


if __name__ == "__main__":
    HackEegTestApplication().main()
