#!/usr/bin/env python

import argparse
import cmd
import sys
import time

import hackeeg
from hackeeg import ads1299


# TODO
# - argparse commandline arguments
# - JSONLines
# -MessagePack

class HackEEGShell(cmd.Cmd):
    intro = 'Welcome to the HackEEG shell. Type help or ? to list commands.\n'
    prompt = '(hackeeg) '
    file = None

    def __init__(self):
        super().__init__()
        self.serial_port_name = None
        self.hackeeg = None
        self.debug = False
        self.hex = True

    def _format_response(self, response):
        if self.debug:
            print(f"_format_response: {response}")
        if self.hackeeg.mode == self.hackeeg.JsonLinesMode:
            status_code = response.get(self.hackeeg.StatusCodeKey)
            status_text = response.get(self.hackeeg.StatusTextKey)
            data = response.get(self.hackeeg.DataKey)
        elif self.hackeeg.mode == self.hackeeg.MessagePackMode:
            status_code = response.get(self.hackeeg.MpStatusCodeKey)
            status_text = response.get(self.hackeeg.MpStatusTextKey)
            data = response.get(self.hackeeg.MpDataKey)
        if status_code and status_code == hackeeg.Status.Ok:
            print("Ok", end='')
            if data:
                print(f"  Data: ", end='')
                if isinstance(data, list):
                    self._format_list(data)
                elif isinstance(data, int):
                    self._format_int(data)
                else:
                    print(f"{data}", end='')
            print()
        else:
            print(f"Error: {response}")

    def _format_list(self, data):
        if len(data) == 0:
            return
        for item in data:
            self._format_int(data)

    def _format_int(self, item):
        if self.hex:
            print(f"{item:02x} ", end='')
        else:
            print(f"{item:02d} ", end='')
        print()

    def do_debug(self, arg):
        """Enables debugging output in the client software. Usage: debug <on|off>"""
        usage = "Usage: debug [on|off]"
        if arg == "on":
            self.debug = True
        elif arg == "off":
            self.debug = False
        else:
            print(f"debug flag is currently set to {self.debug}.")
        self.hackeeg.set_debug(self.debug)
        return

    def do_hex(self, arg):
        """Sets numerical output format to hexidecimal or decimal. Defaults to hexidecimal. Usage: hex <on|off>"""
        usage = "Usage: hexmode [on|off]"
        if arg == "on":
            self.hex = True
        elif arg == "off":
            self.hex = False
        print(self._hex_mode_string())
        return

    def _hex_mode_string(self):
        return f"Numerical output is currently set to {'hexidecimal' if self.hex else 'decimal'}."

    def do_nop(self, arg):
        """No operation – does nothing."""
        self._format_response(self.hackeeg.nop())

    def do_version(self, arg):
        """Returns HackEEG driver version number."""
        self._format_response(self.hackeeg.version())

    def do_status(self, arg):
        """Returns HackEEG driver status."""
        self._format_response(self.hackeeg.status())

    def do_text(self, arg):
        """Sets driver communication protocol to text and exits the HackEEG command shell. 
           Useful if you want to communicate directly with the driver using text mode."""
        self._format_response(self.hackeeg.text_mode())

    def do_jsonlines(self, arg):
        """Sets driver communication protocol to jsonlines. (Not implemented yet.)"""
        self._format_response(self.hackeeg.jsonlines_mode())

    def do_messagepack(self, arg):
        """Sets driver communication protocol to MessagePack. (Not implemented yet.)"""
        self._format_response(self.hackeeg.messagepack_mode())

    def do_micros(self, arg):
        """Returns microseconds since the Arduino booted up."""
        self._format_response(self.hackeeg.micros())

    def do_serialnumber(self, arg):
        """Returns board serial number. (Not implemented yet.)"""
        self._format_response(self.hackeeg.serialnumber())

    def do_boardledon(self, arg):
        """Turns the HackEEG board LED on."""
        self._format_response(self.hackeeg.boardledon())

    def do_boardledoff(self, arg):
        """Turns the HackEEG board LED off."""
        self._format_response(self.hackeeg.boardledoff())

    def do_ledon(self, arg):
        """Turns the Arduino LED on."""
        self._format_response(self.hackeeg.ledon())

    def do_ledoff(self, arg):
        """Turns the Arduino LED off."""
        self._format_response(self.hackeeg.ledoff())

    def do_blink(self, arg):
        """Turns the HackEEG board LED off, then on, then off."""
        self.hackeeg.boardledoff()
        self.hackeeg.boardledon()
        time.sleep(0.2)
        self._format_response(self.hackeeg.boardledoff())

    def do_rreg(self, arg):
        """Reads an ADS1299 register and returns the value."""
        usage = "rreg [register_number]"
        arglist = []
        try:
            arglist = parse_registers(arg)
        except HackEEGArgumentException as e:
            pass
        if len(arglist) == 0:
            print("No register number given.")
            print(usage)
        elif len(arglist) > 1:
            print("Too many arguments.")
            print(usage)
        else:
            self._format_response(self.hackeeg.rreg(arglist[0]))

    def do_wreg(self, arg):
        """Writees an ADS1299 register."""
        arglist = parse_registers(arg)
        if len(arglist) == 0:
            print("No register number given.")
        elif len(arglist) == 1:
            print("No register value given.")
        elif len(arglist) > 2:
            print("Too many arguments.")
        else:
            self._format_response(self.hackeeg.wreg(arglist[0], arglist[1]))

    def do_wakeup(self, arg):
        """Sends the WAKEUP command to the ADS1299. (Not implemented yet.)"""
        self._format_response(self.hackeeg.nop())

    def do_standby(self, arg):
        """Sends the STANDBY command to the ADS1299. (Not implemented yet.)"""
        self._format_response(self.hackeeg.nop())

    def do_reset(self, arg):
        """Sends the RESET command to the ADS1299."""
        self._format_response(self.hackeeg.reset())

    def do_start(self, arg):
        """Sends the START command to the ADS1299. (Not implemented yet.)"""
        self._format_response(self.hackeeg.start())

    def do_stop(self, arg):
        """Sends the STOP command to the ADS1299."""
        self._format_response(self.hackeeg.stop())

    def do_rdatac(self, arg):
        """Sends the RDATAC command to the ADS1299."""
        result = self.hackeeg.rdatac()
        if self.hackeeg.ok(result):
            print("rdatac mode on.")
        else:
            print(f"rdatac failed to turn on: {result}")
        self._format_response(result)

    def do_sdatac(self, arg):
        """Sends the SDATAC command to the ADS1299."""
        self._format_response(self.hackeeg.sdatac())

    def do_rdata(self, arg):
        """Sends the RDATA command to the ADS1299. (Not implemented yet.)"""
        self._format_response(self.hackeeg.nop())

    def do_getdata(self, arg):
        """Gets data from the HackEEG driver's ringbuffer. (Not implemented yet.)"""
        self._format_response(self.hackeeg.nop())

    def do_setup(self, arg):
        """Sets up the ADS1299."""
        self.hackeeg.blink_board_led()
        self.hackeeg.reset()
        sample_mode = ads1299.HIGH_RES_250_SPS | ads1299.CONFIG1_const
        self.hackeeg.wreg(ads1299.CONFIG1, sample_mode)
        self.hackeeg.disable_channel(1)
        self.hackeeg.disable_channel(2)
        self.hackeeg.disable_channel(3)
        self.hackeeg.disable_channel(4)
        #self.hackeeg.disable_channel(5)
        self.hackeeg.disable_channel(6)
        #self.hackeeg.disable_channel(7)
        self.hackeeg.disable_channel(8)
        self.hackeeg.wreg(ads1299.CH5SET, ads1299.TEST_SIGNAL | ads1299.GAIN_2X)
        #self.hackeeg.wreg(ads1299.CH7SET, ads1299.TEMP | ads1299.GAIN_12X)
        self.hackeeg.rreg(ads1299.CH5SET)
        #self.hackeeg.wreg(ads1299.CH8SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        self.hackeeg.wreg(ads1299.CH7SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_2X)
        #self.hackeeg.wreg(ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        self.hackeeg.rreg(ads1299.CH8SET)
        #self.hackeeg.rreg(ads1299.CH2SET)
        self.hackeeg.wreg(ads1299.CH2SET, ads1299.ELECTRODE_INPUT | ads1299.GAIN_24X)
        # Unipolar mode - setting SRB1 bit sends mid-supply voltage to the N inputs
        self.hackeeg.wreg(ads1299.MISC1, ads1299.SRB1)
        # add channels into bias generation
        self.hackeeg.wreg(ads1299.BIAS_SENSP, ads1299.BIAS8P)
        self.hackeeg.rdatac()
        return

    def do_exit(self, arg):
        """Exit the HackEEG commandline."""
        sys.exit(0)

    def do_quit(self, arg):
        """Exit the HackEEG commandline."""
        sys.exit(0)

    def default(self, line):
        if line == 'EOF':
            sys.exit(0)

    def setup(self, samplesPerSecond=500):
        if samplesPerSecond not in hackeeg.SPEEDS.keys():
            raise hackeeg.HackEEGException("{} is not a valid speed; valid speeds are {}".format(
                samplesPerSecond, sorted(hackeeg.SPEEDS.keys())))
        self.hackeeg.jsonlines_mode()
        # self.hackeeg.sdatac()
        time.sleep(1)
        # self.hackeeg.send("reset")
        self.hackeeg.blink_board_led()
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
        self.hackeeg.connect()
        intro_message = "Welcome to the HackEEG shell. Type help or ? to list commands.\n"
        intro_message += self._hex_mode_string() + '\n'
        self.cmdloop(intro_message)

class HackEEGArgumentException(Exception):
    pass

def parse_registers(arg):
    """Convert a series of zero or more numbers to an argument tuple"""
    result = tuple(map(int, arg.split()))
    for item in result:
        if item > 255 or item < 0:
            raise HackEEGArgumentException("Register and value must be an integer between 0 and 255.")
    return result


if __name__ == "__main__":
    HackEEGShell().main()
