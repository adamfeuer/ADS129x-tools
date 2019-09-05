#!/usr/bin/env python

import cmd
import sys
import time

import hackeeg


# TODO
# - argparse commandline arguments
# - JSONLines
# -MessagePack

class HackEEGShell(cmd.Cmd):
    intro = 'Welcome to the HackEEG shell.   Type help or ? to list commands.\n'
    prompt = '(hackeeg) '
    file = None

    def __init__(self):
        super().__init__()
        self.serialPortName = None
        self.hackeeg = None

    def _format_response(self, response):
        status_code = response.get(self.hackeeg.StatusCode)
        status_text = response.get(self.hackeeg.StatusText)
        data = response.get(self.hackeeg.Data)
        if status_code and status_code == hackeeg.Status.Ok:
            print("Ok", end='')
            if data: 
                print(f"  Data: {data}", end='')
            print()
        else:
            print("Error")

    def do_nop(self, arg):
        'No operation – does nothing.'
        self._format_response(self.hackeeg.nop())

    def do_version(self, arg):
        'Returns HackEEG driver version number.'
        self._format_response(self.hackeeg.version())

    def do_status(self, arg):
        'Returns HackEEG driver status. (Not implemented yet.)'
        self._format_response(self.hackeeg.status())

    def do_text(self, arg):
        'Sets driver communication protocol to text and exits the HackEEG command shell. Useful if you want to communicate directly with the driver using text mode. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_jsonlines(self, arg):
        'Sets driver communication protocol to jsonlines. (Not implemented yet.)'
        self._format_response(self.hackeeg.jsonlines_mode())

    def do_messagepack(self, arg):
        'Sets driver communication protocol to MessagePack. (Not implemented yet.)'
        self._format_response(self.hackeeg.messagepack())

    def do_micros(self, arg):
        'Returns microseconds since the Arduino booted up.'
        self._format_response(self.hackeeg.micros())

    def do_serialnumber(self, arg):
        'Returns board serial number. (Not implemented yet.)'
        self._format_response(self.hackeeg.serialnumber())

    def do_boardledon(self, arg):
        'Turns the HackEEG board LED on.'
        self._format_response(self.hackeeg.boardledon())

    def do_boardledoff(self, arg):
        'Turns the HackEEG board LED off.'
        self._format_response(self.hackeeg.boardledoff())

    def do_ledon(self, arg):
        'Turns the Arduino LED on.'
        self._format_response(self.hackeeg.ledon())

    def do_ledoff(self, arg):
        'Turns the Arduino LED off.'
        self._format_response(self.hackeeg.ledoff())

    def do_blink(self, arg):
        'Turns the HackEEG board LED off, then on, then off.'
        self.hackeeg.boardledoff()
        self.hackeeg.boardledon()
        time.sleep(0.2)
        self._format_response(self.hackeeg.boardledoff())

    def do_rreg(self, arg):
        'Reads an ADS1299 register and returns the value.'
        arglist = parse(arg)
        if len(arglist) == 0:
            raise HackEEGArgumentException("No register number given.")
        if len(arglist) > 1:
            raise HackEEGArgumentException("Too many arguments.")
        self._format_response(self.hackeeg.rreg(arglist[0]))

    def do_wreg(self, arg):
        'Writees an ADS1299 register.'
        arglist = parse(arg)
        if len(arglist) == 0:
            raise HackEEGArgumentException("No register number given.")
        if len(arglist) == 1:
            raise HackEEGArgumentException("No register value given.")
        if len(arglist) > 2:
            raise HackEEGArgumentException("Too many arguments.")
        self._format_response(self.hackeeg.wreg(arglist[0], arglist[1]))

    def do_wakeup(self, arg):
        'Sends the WAKEUP command to the ADS1299. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_standby(self, arg):
        'Sends the STANDBY command to the ADS1299. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_reset(self, arg):
        'Sends the RESET command to the ADS1299. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_start(self, arg):
        'Sends the START command to the ADS1299. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_stop(self, arg):
        'Sends the STOP command to the ADS1299. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_rdatac(self, arg):
        'Sends the RDATAC command to the ADS1299. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_sdatac(self, arg):
        'Sends the SDATAC command to the ADS1299. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_rdata(self, arg):
        'Sends the RDATA command to the ADS1299. (Not implemented yet.)'
        self._format_response(self.hackeeg.nop())

    def do_getdata(self, arg):
        "Gets data from the HackEEG driver's ringbuffer. (Not implemented yet.)"
        self._format_response(self.hackeeg.nop())

    def do_exit(self, arg):
        'Exit the HackEEG commandline.'
        sys.exit(0)

    def do_quit(self, arg):
        'Exit the HackEEG commandline.'
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
        self.serialPortName = sys.argv[1]
        self.hackeeg = hackeeg.HackEEGBoard(self.serialPortName, debug=False)
        self.cmdloop()

class HackEEGArgumentException(Exception):
    pass

def parse(arg):
    'Convert a series of zero or more numbers to an argument tuple'
    result = tuple(map(int, arg.split()))
    for item in result:
        if item > 255 or item < 0:
            raise HackEEGArgumentException("Register and value must be an integer between 0 and 255.")
    return result


if __name__ == "__main__":
    HackEEGShell().main()
