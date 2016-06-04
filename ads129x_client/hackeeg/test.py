#!/usr/bin/env python

import time
from driver import HackEegBoard

class Test():
   def __init__(self):
      pass

   def main(self):
      h = HackEegBoard(serialPortPath = "/dev/tty.usbmodem1d111", baudrate = 115200)
      h.setup()
      while True:
         print h.readSample()

if __name__ == "__main__":
   Test().main()
