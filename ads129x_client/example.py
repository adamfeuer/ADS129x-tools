#!/usr/bin/env python
# This is the example used in the documentation.

SERIAL_PORT_PATH="/dev/cu.usbmodem14434401"  # your actual path to the Arduino Native serial port device goes here
import sys

import hackeeg
from hackeeg import ads1299

hackeeg = hackeeg.HackEEGBoard(SERIAL_PORT_PATH)
hackeeg.connect()
hackeeg.sdatac()
hackeeg.reset()
hackeeg.blink_board_led()
hackeeg.disable_all_channels()
sample_mode = ads1299.HIGH_RES_250_SPS | ads1299.CONFIG1_const
hackeeg.wreg(ads1299.CONFIG1, sample_mode)
test_signal_mode = ads1299.INT_TEST_4HZ | ads1299.CONFIG2_const
hackeeg.wreg(ads1299.CONFIG2, test_signal_mode)
hackeeg.enable_channel(7)
hackeeg.wreg(ads1299.CH7SET, ads1299.TEST_SIGNAL | ads1299.GAIN_1X)
hackeeg.rreg(ads1299.CH5SET)

# Unipolar mode - setting SRB1 bit sends mid-supply voltage to the N inputs
hackeeg.wreg(ads1299.MISC1, ads1299.SRB1)
# add channels into bias generation
hackeeg.wreg(ads1299.BIAS_SENSP, ads1299.BIAS8P)
hackeeg.rdatac()
hackeeg.start()

while True:
    result = hackeeg.read_response()
    status_code = result.get('STATUS_CODE')
    status_text = result.get('STATUS_TEXT')
    data = result.get(hackeeg.DataKey)
    if data:
        decoded_data = result.get(hackeeg.DecodedDataKey)
        if decoded_data:
            timestamp = decoded_data.get('timestamp')
            ads_gpio = decoded_data.get('ads_gpio')
            loff_statp = decoded_data.get('loff_statp')
            loff_statn = decoded_data.get('loff_statn')
            channel_data = decoded_data.get('channel_data')
            print(f"timestamp:{timestamp} | gpio:{ads_gpio} loff_statp:{loff_statp} loff_statn:{loff_statn} |   ",
                  end='')
            for channel_number, sample in enumerate(channel_data):
                print(f"{channel_number + 1}:{sample} ", end='')
            print()
        else:
            print(data)
        sys.stdout.flush()

