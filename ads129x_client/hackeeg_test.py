#!/usr/bin/env python

import argparse

import hackeeg
from hackeeg import ads1299
from hackeeg.driver import SPEEDS


# TODO
# - argparse commandline arguments

class HackEegTestApplication:
    def __init__(self):
        self.serial_port_name = None
        self.hackeeg = None
        self.debug = False

    def setup(self, samples_per_second=500):
        if samples_per_second not in SPEEDS.keys():
            raise HackEegException("{} is not a valid speed; valid speeds are {}".format(
                samples_per_second, sorted(SPEEDS.keys())))
        self.hackeeg.sdatac()
        self.hackeeg.reset()
        self.hackeeg.blink_board_led()
        self.hackeeg.disable_all_channels()
        sample_mode = ads1299.HIGH_RES_250_SPS | ads1299.CONFIG1_const
        self.hackeeg.wreg(ads1299.CONFIG1, sample_mode)
        test_signal_mode = ads1299.INT_TEST_4HZ | ads1299.CONFIG2_const
        self.hackeeg.wreg(ads1299.CONFIG2, test_signal_mode)
        #self.hackeeg.enable_channel(5)
        self.hackeeg.enable_channel(7)
        #self.hackeeg.wreg(ads1299.CH5SET, ads1299.TEST_SIGNAL | ads1299.GAIN_8X)
        #self.hackeeg.wreg(ads1299.CH5SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        self.hackeeg.wreg(ads1299.CH7SET, ads1299.TEST_SIGNAL | ads1299.GAIN_1X)
        # self.hackeeg.wreg(ads1299.CH7SET, ads1299.TEMP | ads1299.GAIN_12X)
        self.hackeeg.rreg(ads1299.CH5SET)
        #self.hackeeg.wreg(ads1299.CH8SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        #self.hackeeg.wreg(ads1299.CH7SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_1X)
        #self.hackeeg.wreg(ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)

        # Unipolar mode - setting SRB1 bit sends mid-supply voltage to the N inputs
        self.hackeeg.wreg(ads1299.MISC1, ads1299.SRB1)
        # add channels into bias generation
        self.hackeeg.wreg(ads1299.BIAS_SENSP, ads1299.BIAS8P)
        self.hackeeg.rdatac()
        self.hackeeg.start()
        return

    def main(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("--serial_port", "-p", help="serial port device path",
                            type=str)
        parser.add_argument("--debug", "-d", help="enable debugging output",
                            action="store_true")
        args = parser.parse_args()
        if args.debug:
            self.debug = True
            print("debug mode on")
        self.serial_port_name = args.serial_port
        self.hackeeg = hackeeg.HackEEGBoard(self.serial_port_name, debug=self.debug)
        self.setup()
        while True:
            result = self.hackeeg.read_response()
            status_code = result.get('STATUS_CODE')
            status_text = result.get('STATUS_TEXT')
            data = result.get('DATA')
            # ADS1299 sample status bits - datasheet, p36
            # 1100 + LOFF_STATP + LOFF_STATN + bits[4:7]of the GPIOregister)
            if data:
                timestamp = int.from_bytes(data[0:4], byteorder='little')
                ads_status = int.from_bytes(data[4:7], byteorder='big')
                ads_gpio = ads_status & 0x0f
                loff_statn = (ads_status >> 4) & 0xff
                loff_statp = (ads_status >> 12) & 0xff
                extra = (ads_status >> 20) & 0xff

                channel_samples = []
                for channel in range(0, 8):
                    channel_offset = 7 + (channel * 3)
                    sample = int.from_bytes(data[channel_offset:channel_offset + 3], byteorder='little')
                    channel_samples.append(sample)

                print(f"timestamp:{timestamp} | gpio:{ads_gpio} loff_statp:{loff_statp} loff_statn:{loff_statn}", end='')
                for channel_number, sample in enumerate(channel_samples):
                    print(f"{channel_number+1}:{sample} ", end='')
                print()


if __name__ == "__main__":
    HackEegTestApplication().main()
