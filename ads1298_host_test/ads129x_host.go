// ADS1298 test program
//
// this seems to be good up to about 7800 samples per second
// on my 2012 MacBook Pro

package main

import (
      "github.com/tarm/goserial"
      "log"
      "fmt"
      "flag"
      "io"
      "bufio"
      "time"
      "strings"
)

var testDuration time.Duration = 10 * time.Second
var port string
var baud int

// ADS1298 constants
var (
    HR int = 0x80
    DR2 int = 0x04
    DR1 int = 0x02
    DR0 int = 0x01
    CONFIG1_const int = 0x00
    HIGH_RES_32k_SPS int = HR
    HIGH_RES_16k_SPS int = (HR | DR0)
    HIGH_RES_8k_SPS int = (HR | DR1)
    HIGH_RES_4k_SPS int = (HR | DR1 | DR0)
    HIGH_RES_2k_SPS int = (HR | DR2)
    HIGH_RES_1k_SPS int = (HR | DR2 | DR0)
    HIGH_RES_500_SPS int = (HR | DR2 | DR1)
    LOW_POWR_250_SPS int = (DR2 | DR1)
)
func init() {
    flag.StringVar(&port, "port", "[undefined]", "path to serial port device")
    flag.IntVar(&baud, "baud", 115200, "baud rate to use with serial port device")
}

func readLine(serialReader *bufio.Reader) (result string) {
    lineBytes, err := serialReader.ReadBytes('\n')
    result = strings.TrimRight(string(lineBytes), "\n")
    if err != nil {
        fmt.Println(err)
        return
    }
    return
}

func send(serialWriter io.Writer, command string) {
    n, err := serialWriter.Write([]byte(command + "\n"))
    if err != nil {
            log.Fatal(err)
    }
    if (n==0) {}
}

var inCount int
func reader(serialReader *bufio.Reader, lineChannel chan string) {
    for {
        lineChannel <- readLine(serialReader)
        inCount++
    }
}

var outCount int
func printer(lineChannel chan string) {
    for {
        line := <- lineChannel
        outCount++
        fmt.Printf("%s\n", line) 
    }
}

func main() {
    flag.Parse()
    fmt.Printf("Hello, world.\n")
    fmt.Printf("Serial port: %s\n", port)
    fmt.Printf("Baud rate: %d\n", baud)

    c := &serial.Config{Name: port, Baud: baud}
    ser, err := serial.OpenPort(c)
    if err != nil {
            log.Fatal(err)
    }

    serialReader := bufio.NewReader(ser)
    serialWriter := ser

    send(serialWriter, "sdatac")
    fmt.Printf("%s", readLine(serialReader))
    fmt.Printf("%s", readLine(serialReader))
    send(serialWriter, "version")
    fmt.Printf("%s", readLine(serialReader))
    fmt.Printf("%s", readLine(serialReader))
    mode := fmt.Sprintf("%0x", HIGH_RES_8k_SPS)
    send(serialWriter, "wreg 01 " + mode) 
    fmt.Printf("%s", readLine(serialReader))
    send(serialWriter, "rdatac")
    fmt.Printf("%s", readLine(serialReader))
    fmt.Printf("%s", readLine(serialReader))

    lineChannel := make(chan string, 1000000)
    go reader(serialReader, lineChannel)
    go printer(lineChannel)
    time.Sleep(testDuration)
    var seconds float64 = testDuration.Seconds()
    var sps float64 = float64(inCount) / seconds 
    fmt.Printf("%d lines in %f seconds; %f lines per second.", inCount, seconds, sps)

}
