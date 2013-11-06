// ADS1298 test program
//
// this progam can read 8000 samples per second
// and base64 decode them on my 2012 MacBook Pro

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
      "runtime"
      "encoding/base64"
)

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

var (
    NCPU int = 1 
    BUFFER_SIZE int = 1000
    SAMPLING_MODE int = HIGH_RES_8k_SPS
    testDuration time.Duration = 10 * time.Second
    port string
    baud int
    tempBuffer = make([]byte, 10000)
    //NCPU int = runtime.NumCPU() // use the maximum number of CPUs for parallelization
    byteCount int
    sampleCount int
)


func init() {
    flag.StringVar(&port, "port", "[undefined]", "path to serial port device")
    flag.IntVar(&baud, "baud", 115200, "baud rate to use with serial port device")
}

func readChunk(serialReader *bufio.Reader, bufferedWritePipe *bufio.Writer) (err error) {
    chunkLen, err := serialReader.Read(tempBuffer)
    if err != nil && err == io.EOF {
        return err
    }
    if chunkLen > 0 {
        bufferedWritePipe.Write(tempBuffer[:chunkLen])
        byteCount += chunkLen
    }
    return nil
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
    fmt.Println(command)
    n, err := serialWriter.Write([]byte(command + "\n"))
    if err != nil {
            log.Fatal(err)
    }
    _ = n
    time.Sleep(100 * time.Millisecond)
}

func reader(serialReader *bufio.Reader, bufferedWritePipe *bufio.Writer, quit chan bool) {
    Loop:     
    for {
        select {
             case <- quit:
                 break Loop
             default:
                 err := readChunk(serialReader, bufferedWritePipe)
                 if (err != nil && err == io.EOF) {
                     break Loop
                 }
         }
    }
}

func decoder(bufferedReadPipe *bufio.Reader, quit chan bool) {
    Loop:     
    for {
        select {
             case <- quit:
                 break Loop
             default:
                 line, isPrefix, err := bufferedReadPipe.ReadLine()
                 //fmt.Printf("%s\n", line) // printing the samples radically reduces sample rate
                 _ = isPrefix
                 if (err != nil && err == io.EOF) {
                     break Loop
                 }
                 sampleCount++
                 sample := make([]byte, 100)
                 sampleLength, err := base64.StdEncoding.Decode(sample, line)
                 _ = sampleLength
                 if err != nil {
                        fmt.Println("error:", err)
                 }
         }
    }
}

func main() {
    fmt.Printf("Number of reported CPUs: %d\n", runtime.NumCPU())
    fmt.Printf("Using CPUs: %d\n", NCPU)
    runtime.GOMAXPROCS(NCPU)    

    flag.Parse()
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
    time.Sleep(1 * time.Second)
    fmt.Printf("%s\n", readLine(serialReader))
    fmt.Printf("%s\n", readLine(serialReader))
    send(serialWriter, "version")
    fmt.Printf("%s\n", readLine(serialReader))
    fmt.Printf("%s\n", readLine(serialReader))
    mode := fmt.Sprintf("%0x", SAMPLING_MODE)
    send(serialWriter, "wreg 01 " + mode) 
    fmt.Printf("%s\n", readLine(serialReader))
    send(serialWriter, "rdatac")
    fmt.Printf("%s\n", readLine(serialReader))
    fmt.Printf("%s\n", readLine(serialReader))

    readPipe, writePipe := io.Pipe()
    bufferedReadPipe := bufio.NewReaderSize(readPipe, BUFFER_SIZE)
    bufferedWritePipe := bufio.NewWriterSize(writePipe, BUFFER_SIZE)

    readerMessages := make(chan bool)
    decoderMessages := make(chan bool)

    quit := true
    go reader(serialReader, bufferedWritePipe, readerMessages)
    go decoder(bufferedReadPipe, decoderMessages)
    time.Sleep(testDuration)
    readerMessages <- quit 
    time.Sleep(300 * time.Millisecond)

    send(serialWriter, "sdatac")
    line := ""
    count := 0
    for (strings.HasPrefix(line, "200") != true) {
       line = readLine(serialReader)
       fmt.Printf(".")
       count++
    }
    fmt.Printf("\n")
    fmt.Printf("Samples read after giving SDATAC command: %d\n", count)

    var seconds float64 = testDuration.Seconds()
    var bps float64 = float64(byteCount) / seconds
    fmt.Printf("\n\n")
    fmt.Printf("%d bytes in %f seconds; %f bytes per second.\n", byteCount, seconds, bps)
    var sps float64 = float64(sampleCount) / seconds
    fmt.Printf("%d samples in %f seconds; %f samples per second.\n", sampleCount, seconds, sps)

}
