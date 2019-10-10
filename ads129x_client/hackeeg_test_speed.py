#!/usr/bin/env python

import argparse
import sys
import time

import hackeeg
from hackeeg import ads1299
from hackeeg.driver import SPEEDS, Status


class HackEegTestApplication:
    """Basic commandline test application – sets a test waveform on Channel 7, enters RDATAC (read data continuous) mode,
       and outputs the samples received."""
    def __init__(self):
        self.serial_port_name = None
        self.hackeeg = None
        self.debug = False
        # self.debug = True

    def setup(self, samples_per_second=500):
        if samples_per_second not in SPEEDS.keys():
            raise HackEegException("{} is not a valid speed; valid speeds are {}".format(
                samples_per_second, sorted(SPEEDS.keys())))
        self.hackeeg.jsonlines_mode()
        self.hackeeg.sdatac()
        self.hackeeg.reset()
        self.hackeeg.blink_board_led()
        self.hackeeg.disable_all_channels()
        # sample_mode = ads1299.HIGH_RES_250_SPS | ads1299.CONFIG1_const
        sample_mode = ads1299.HIGH_RES_16k_SPS | ads1299.CONFIG1_const
        #sample_mode = ads1299.HIGH_RES_8k_SPS | ads1299.CONFIG1_const
        #sample_mode = ads1299.HIGH_RES_4k_SPS | ads1299.CONFIG1_const
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
        # self.hackeeg.messagepack_mode()
        self.hackeeg.start()
        self.hackeeg.rdatac()
        return

    def main(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("serial_port", help="serial port device path",
                            type=str)
        parser.add_argument("--debug", "-d", help="enable debugging output",
                            action="store_true")
        args = parser.parse_args()
        if args.debug:
            self.debug = True
            print("debug mode on")
        self.serial_port_name = args.serial_port
        self.hackeeg = hackeeg.HackEEGBoard(self.serial_port_name, baudrate=2000000, debug=self.debug)
        self.hackeeg.connect()
        self.setup()
        # while True:
        # max_samples = 480000
        max_samples = 50000
        start_time = time.perf_counter()
        samples = 0
        while samples < max_samples:
            samples += 1
            result = self.hackeeg.read_rdatac_response()
            if result:
                status_code = result.get(self.hackeeg.MpStatusCodeKey)
                data = result.get(self.hackeeg.MpDataKey)
                if status_code == Status.Ok and data:
                    decoded_data = result.get(self.hackeeg.DecodedDataKey)
                    if decoded_data:
                        timestamp = decoded_data.get('timestamp')
                        ads_gpio = decoded_data.get('ads_gpio')
                        loff_statp = decoded_data.get('loff_statp')
                        loff_statn = decoded_data.get('loff_statn')
                        channel_data = decoded_data.get('channel_data')
            else:
                print("no data to decode")
                print(f"result: {result}")
        end_time = time.perf_counter()
        duration = end_time - start_time
        self.hackeeg.sdatac()
        self.hackeeg.stop()
        self.hackeeg.blink_board_led()
        print(f"duration in seconds: {duration}")
        samples_per_second = max_samples/duration
        print(f"samples per second: {samples_per_second}")


if __name__ == "__main__":
    HackEegTestApplication().main()
