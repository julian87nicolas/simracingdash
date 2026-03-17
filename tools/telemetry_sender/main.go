package main

import (
    "encoding/binary"
    "flag"
    "log"
    "math"
    "math/rand"
    "net"
    "time"
)

var addr = flag.String("addr", "127.0.0.1:20777", "UDP address to send telemetry to")

func makeCarTelemetryPacket(speed uint16, rpm uint16, throttle byte, brake byte, gear int8, drs byte, mfd byte) []byte {
    buf := make([]byte, 64)
    // common header: set packet id at offset 5
    buf[5] = 6 // CarTelemetry (as used in parser)
    // write speed at offset 24 (uint16 little-endian)
    binary.LittleEndian.PutUint16(buf[24:], speed)
    // rpm at offset 26
    binary.LittleEndian.PutUint16(buf[26:], rpm)
    // throttle & brake at offsets 32,33
    buf[32] = throttle
    buf[33] = brake
    // gear at 36
    buf[36] = byte(gear)
    // drs at 37
    buf[37] = drs
    // mfd panel at 38
    buf[38] = mfd
    return buf
}

func main() {
    flag.Parse()
    raddr, err := net.ResolveUDPAddr("udp", *addr)
    if err != nil {
        log.Fatalf("resolve error: %v", err)
    }
    conn, err := net.DialUDP("udp", nil, raddr)
    if err != nil {
        log.Fatalf("dial error: %v", err)
    }
    defer conn.Close()

    log.Printf("Sending telemetry to %s", raddr.String())
    t := time.NewTicker(100 * time.Millisecond)
    defer t.Stop()
    start := time.Now()
    for i := 0; ; i++ {
        <-t.C
        // generate oscillating values
        elapsed := time.Since(start).Seconds()
        speed := uint16(50 + 100*math.Abs(math.Sin(elapsed*0.5)))
        rpm := uint16(3000 + 9000*math.Abs(math.Sin(elapsed*0.8)))
        throttle := byte(128 + 127*math.Sin(elapsed*1.2))
        brake := byte(0)
        if rand.Float32() < 0.02 {
            // occasional braking pulse
            brake = 200
        }
        gear := int8(1 + int((elapsed/5))%8)
        drs := byte(0)
        if rpm > 9000 && rand.Float32() < 0.3 {
            drs = 1
        }
        mfd := byte(i/50%5) // cycle panels every ~5s

        pkt := makeCarTelemetryPacket(speed, rpm, throttle, brake, gear, drs, mfd)
        _, err := conn.Write(pkt)
        if err != nil {
            log.Printf("write error: %v", err)
            continue
        }
    }
}
