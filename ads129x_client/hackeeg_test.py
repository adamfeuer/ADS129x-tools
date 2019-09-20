#!/usr/bin/env python

import argparse

import hackeeg
from hackeeg import ads1299
from hackeeg.driver import SPEEDS


class HackEegTestApplication:
    """Basic commandline test application – sets a test waveform on Channel 7, enters RDATAC (read data continuous) mode,
       and outputs the samples received."""
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
        parser.add_argument("serial_port", help="serial port device path",
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
            data = result.get(self.hackeeg.DataKey)
            if data:
                decoded_data = result.get(self.hackeeg.DecodedDataKey)
                if decoded_data:
                    timestamp = decoded_data.get('timestamp')
                    ads_gpio = decoded_data.get('ads_gpio')
                    loff_statp = decoded_data.get('loff_statp')
                    loff_statn = decoded_data.get('loff_statn')
                    channel_data = decoded_data.get('channel_data')
                    print(f"timestamp:{timestamp} | gpio:{ads_gpio} loff_statp:{loff_statp} loff_statn:{loff_statn}   ",
                          end='')
                    for channel_number, sample in enumerate(channel_data):
                        print(f"{channel_number+1}:{sample} ", end='')
                    print()
                else:
                    print(data)


if __name__ == "__main__":
    HackEegTestApplication().main()
