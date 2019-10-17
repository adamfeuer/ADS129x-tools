#!/usr/bin/env python

import argparse
import threading
import uuid
import time
import sys
import select
import tty
import termios

from pylsl import StreamInfo, StreamOutlet

import hackeeg
from hackeeg import ads1299
from hackeeg.driver import SPEEDS, Status

DEFAULT_NUMBER_OF_SAMPLES_TO_CAPTURE = 50000


class HackEegTestApplicationException(Exception):
    pass


class NonBlockingConsole(object):

    def __enter__(self):
        self.old_settings = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
        return self

    def __exit__(self, type, value, traceback):
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)


    def get_data(self):
        if select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
            return sys.stdin.read(1)
        return False


class HackEegTestApplication:
    """HackEEG commandline test."""

    def __init__(self):
        self.serial_port_name = None
        self.hackeeg = None
        self.debug = False
        self.quiet = False
        self.channels = 8
        self.samples_per_second = 500
        self.lsl = False
        self.lsl_info = None
        self.lsl_outlet = None
        self.lsl_stream_name = "HackEEG"
        self.stream_id = str(uuid.uuid4())
        self.read_samples_continuously = True
        self.continuous_mode = False
        self.non_blocking_console = NonBlockingConsole()
        # self.debug = True

    def find_dropped_samples(self, samples, number_of_samples):
        sample_numbers = {self.get_sample_number(sample): 1 for sample in samples}
        correct_sequence = {index: 1 for index in range(0, number_of_samples)}
        missing_samples = [sample_number for sample_number in correct_sequence.keys()
                           if sample_number not in sample_numbers]
        return len(missing_samples)

    def get_sample_number(self, sample):
        decoded_data = sample.get(self.hackeeg.DecodedDataKey)
        sample_number = decoded_data.get('sample_number', -1)
        return sample_number

    def read_keyboard_input(self):
        char = self.non_blocking_console.get_data()
        if char:
            self.read_samples_continuously = False

    def setup(self, samples_per_second=8192):
        if samples_per_second not in SPEEDS.keys():
            raise HackEegTestApplicationException("{} is not a valid speed; valid speeds are {}".format(
                samples_per_second, sorted(SPEEDS.keys())))
        self.hackeeg.stop_and_sdatac_messagepack()
        self.hackeeg.sdatac()
        self.hackeeg.reset()
        self.hackeeg.blink_board_led()
        self.hackeeg.disable_all_channels()
        sample_mode = SPEEDS[samples_per_second] | ads1299.CONFIG1_const

        self.hackeeg.wreg(ads1299.CONFIG1, sample_mode)
        test_signal_mode = ads1299.INT_TEST_4HZ | ads1299.CONFIG2_const
        self.hackeeg.wreg(ads1299.CONFIG2, test_signal_mode)

        # one channel enabled
        self.hackeeg.disable_all_channels()
        self.hackeeg.wreg(ads1299.CHnSET + 7, ads1299.TEST_SIGNAL | ads1299.GAIN_2X)

        # all channels enabled
        # for channel in range(1, 9):
        #     self.hackeeg.wreg(ads1299.CHnSET + channel, ads1299.TEST_SIGNAL | ads1299.GAIN_2X)

        # Unipolar mode - setting SRB1 bit sends mid-supply voltage to the N inputs
        self.hackeeg.wreg(ads1299.MISC1, ads1299.SRB1)
        # add channels into bias generation
        self.hackeeg.wreg(ads1299.BIAS_SENSP, ads1299.BIAS8P)
        self.hackeeg.messagepack_mode()
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
        parser.add_argument("--continuous", "-C", help="read data continuously (until <return> key is pressed)",
                            action="store_true")
        parser.add_argument("--sps", "-s",
                            help=f"ADS1299 samples per second setting- must be one of {sorted(list(SPEEDS.keys()))}",
                            default=8192, type=int)
        parser.add_argument("--lsl", "-L",
                            help=f"Send samples to an LSL stream instead of terminal",
                            action="store_true"),
        parser.add_argument("--lsl-stream-name", "-N",
                            help=f"Name of LSL stream to create",
                            default=self.lsl_stream_name, type=str),
        parser.add_argument("--quiet", "-q",
                            help=f"quiet mode– do not print sample data (used for performance testing)",
                            action="store_true")
        args = parser.parse_args()
        if args.debug:
            self.debug = True
            print("debug mode on")
        self.samples_per_second = args.sps

        if args.continuous:
            self.continuous_mode = True

        if args.lsl:
            self.lsl = True
            if args.lsl_stream_name:
                self.lsl_stream_name = args.lsl_stream_name
            self.lsl_info = StreamInfo(self.lsl_stream_name, 'EEG', self.channels, self.samples_per_second, 'int32', self.stream_id)
            self.lsl_outlet = StreamOutlet(self.lsl_info)

        self.serial_port_name = args.serial_port
        self.hackeeg = hackeeg.HackEEGBoard(self.serial_port_name, baudrate=2000000, debug=self.debug)
        self.hackeeg.connect()
        self.setup(samples_per_second=args.sps)
        max_samples = args.samples
        self.quiet = args.quiet
        samples = []
        sample_counter = 0
        end_time = time.perf_counter()
        start_time = time.perf_counter()
        while sample_counter < max_samples and self.read_samples_continuously:
            result = self.hackeeg.read_rdatac_response()
            end_time = time.perf_counter()
            sample_counter += 1
            self.read_keyboard_input()
            if result:
                status_code = result.get(self.hackeeg.MpStatusCodeKey)
                data = result.get(self.hackeeg.MpDataKey)
                samples.append(result)
                if status_code == Status.Ok and data:
                    decoded_data = result.get(self.hackeeg.DecodedDataKey)
                    if decoded_data:
                        timestamp = decoded_data.get('timestamp')
                        sample_number = decoded_data.get('sample_number')
                        ads_gpio = decoded_data.get('ads_gpio')
                        loff_statp = decoded_data.get('loff_statp')
                        loff_statn = decoded_data.get('loff_statn')
                        channel_data = decoded_data.get('channel_data')
                        if not self.quiet:
                            print(f"timestamp:{timestamp} sample_number: {sample_number}| gpio:{ads_gpio} loff_statp:{loff_statp} loff_statn:{loff_statn}   ",
                                  end='')
                            for channel_number, sample in enumerate(channel_data):
                                print(f"{channel_number+1}:{sample} ", end='')
                            print()
                        if self.lsl:
                            self.lsl_outlet.push_sample(channel_data)
                    else:
                        if not self.quiet:
                            print(data)
            else:
                print("no data to decode")
                print(f"result: {result}")
        duration = end_time - start_time
        self.hackeeg.stop_and_sdatac_messagepack()
        self.hackeeg.blink_board_led()
        if self.continuous_mode:
            max_samples = sample_counter
        print(f"duration in seconds: {duration}")
        samples_per_second = max_samples / duration
        print(f"samples per second: {samples_per_second}")
        dropped_samples = self.find_dropped_samples(samples, max_samples)
        print(f"dropped samples: {dropped_samples}")


if __name__ == "__main__":
    HackEegTestApplication().main()
