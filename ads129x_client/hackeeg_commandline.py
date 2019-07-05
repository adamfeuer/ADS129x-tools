#!/usr/bin/env python

import sys
from hackeeg.driver import HackEegBoard


class HackEegCommandLine:
    def __init__(self):
        self.serialPortName = sys.argv[1]
        pass

    def main(self):
        self.hackeeg = HackEegBoard(self.serialPortName)
        self.hackeeg.setup()
        while True:
            sample = self.hackeeg.readSample()
            print("{sample}")


if __name__ == "__main__":
    HackEegCommandLine().main()
