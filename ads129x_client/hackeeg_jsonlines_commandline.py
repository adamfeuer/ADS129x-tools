#!/usr/bin/env python

import sys
import time
import cmd

import hackeeg

commands = [
    "nop",
    "micros",
    "version",
    "status",
    "serialnumber",
    "text",
    "jsonlines",
    "messagepack",
    "ledon",
    "ledoff",
    "boardledoff",
    "boardledon",
    "wakeup",
    "standby",
    "reset",
    "start",
    "stop",
    "rdatac",
    "sdatac",
    "getdata",
    "rdata",
    "rreg",
    "wreg",
    "help",
]

# TODO
# - argparse commandline arguments
# - JSONLines, MessagePack


class HackEEGCommandLine(cmd.Cmd):
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

    def do_micros(self, arg):
        'Returns microseconds since the Arduino booted up.'
        self._format_response(self.hackeeg.micros())

    def do_boardledon(self, arg):
        'Turns the HackEEG board LED on.'
        self._format_response(self.hackeeg.boardledon())

    def do_boardledoff(self, arg):
        'Turns the HackEEG board LED off.'
        self._format_response(self.hackeeg.boardledoff())

    def do_blink(self, arg):
        'Turns the HackEEG board LED off, then on, then off.'
        self.hackeeg.boardledoff()
        self.hackeeg.boardledon()
        time.sleep(0.2)
        self._format_response(self.hackeeg.boardledoff())

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


if __name__ == "__main__":
    HackEEGCommandLine().main()
