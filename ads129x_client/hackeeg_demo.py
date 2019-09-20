#!/usr/bin/env python

import sys
import hackeeg.driver


class HackEegDemo:
    def __init__(self):
        self.serialPortName = sys.argv[1]
        pass

    def main(self):
        self.hackeeg = hackeeg.driver.HackEegBoard(self.serialPortName)
        self.hackeeg.setup()
        while True:
            print "%s" % self.hackeeg.readSample()


if __name__ == "__main__":
    HackEegDemo().main()
