// consumer.go
package main

import (
    "fmt"
    "os"
    "syscall"
    "unsafe"
)

const (
    MSGKEY   = 1234
    MSGSIZE  = 256
)

type Msgbuf struct {
    Mtype int64
    Mtext [MSGSIZE]byte
}

func main() {
    // Open the message queue
    msqid, _, errno := syscall.Syscall(syscall.SYS_MSGGET, uintptr(MSGKEY), uintptr(0666), 0) //0666 perm for message queue
    if errno != 0 {
        fmt.Fprintf(os.Stderr, "Error opening message queue: %v\n", errno)
        os.Exit(1)
    }

    // Receive a message from the queue
    var msg Msgbuf
    _, _, errno = syscall.Syscall6(syscall.SYS_MSGRCV, msqid, uintptr(unsafe.Pointer(&msg)), MSGSIZE, 0, 0, 0)
    if errno != 0 {
        fmt.Fprintf(os.Stderr, "Error receiving message from queue: %v\n", errno)
        os.Exit(1)
    }

    fmt.Printf("Message received from the queue by the consumer: %s\n", string(msg.Mtext[:]))
}
