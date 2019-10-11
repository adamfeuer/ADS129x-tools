#!/usr/bin/env python

import argparse
import time

import hackeeg
from hackeeg import ads1299
from hackeeg.driver import SPEEDS, Status

DEFAULT_NUMBER_OF_SAMPLES_TO_CAPTURE = 50000


class HackEegTestApplicationException(Exception):
    pass


class HackEegTestApplication:
    """HackEEG commandline test."""

    def __init__(self):
        self.serial_port_name = None
        self.hackeeg = None
        self.debug = False
        self.quiet = False
        # self.debug = True

    def setup(self, samples_per_second=8192):
        if samples_per_second not in SPEEDS.keys():
            raise HackEegTestApplicationException("{} is not a valid speed; valid speeds are {}".format(
                samples_per_second, sorted(SPEEDS.keys())))
        self.hackeeg.stop_and_sdatac_messagepack()
        # self.hackeeg.jsonlines_mode()
        self.hackeeg.sdatac()
        self.hackeeg.reset()
        self.hackeeg.blink_board_led()
        self.hackeeg.disable_all_channels()
        sample_mode = SPEEDS[samples_per_second] | ads1299.CONFIG1_const

        self.hackeeg.wreg(ads1299.CONFIG1, sample_mode)
        test_signal_mode = ads1299.INT_TEST_4HZ | ads1299.CONFIG2_const
        self.hackeeg.wreg(ads1299.CONFIG2, test_signal_mode)

        self.hackeeg.disable_all_channels()
        # for channel in range(1, 9):
        # for channel in range(7, 7):
        #     self.hackeeg.enable_channel(channel)
        #     self.hackeeg.wreg(ads1299.CHnSET + channel, ads1299.TEST_SIGNAL | ads1299.GAIN_1X)

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
        parser.add_argument("--samples", "-S", help="how many samples to capture",
                            default=DEFAULT_NUMBER_OF_SAMPLES_TO_CAPTURE, type=int)
        parser.add_argument("--sps", "-s",
                            help=f"ADS1299 samples per second setting- must be one of {sorted(list(SPEEDS.keys()))}",
                            default=8192, type=int)
        parser.add_argument("--quiet", "-q",
                            help=f"quiet mode– do not print sample data (used for performance testing)",
                            action="store_true")
        args = parser.parse_args()
        if args.debug:
            self.debug = True
            print("debug mode on")
        self.serial_port_name = args.serial_port
        self.hackeeg = hackeeg.HackEEGBoard(self.serial_port_name, baudrate=2000000, debug=self.debug)
        self.hackeeg.connect()
        self.setup(samples_per_second=args.sps)
        max_samples = args.samples
        start_time = time.perf_counter()
        end_time = time.perf_counter()
        samples = 0
        self.quiet = args.quiet
        while samples < max_samples:
            samples += 1
            result = self.hackeeg.read_rdatac_response()
            end_time = time.perf_counter()
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
                        if not self.quiet:
                            print(f"timestamp:{timestamp} | gpio:{ads_gpio} loff_statp:{loff_statp} loff_statn:{loff_statn}   ",
                                  end='')
                            for channel_number, sample in enumerate(channel_data):
                                print(f"{channel_number+1}:{sample} ", end='')
                            print()
                    else:
                        if not self.quiet:
                            print(data)
            else:
                print("no data to decode")
                print(f"result: {result}")
        duration = end_time - start_time
        self.hackeeg.stop_and_sdatac_messagepack()
        self.hackeeg.blink_board_led()
        print(f"duration in seconds: {duration}")
        samples_per_second = max_samples / duration
        print(f"samples per second: {samples_per_second}")


if __name__ == "__main__":
    HackEegTestApplication().main()
