// producer.go
package main

import (
    "fmt"
    "os"
    "syscall"
    "unsafe" //convert betn go types and pointers
)

const (
    MSGKEY   = 1234 //msg queue identifier(0x4d2)
    MSGSIZE  = 256
    IPC_CREAT = 01000
)

type Msgbuf struct {
    Mtype int64
    Mtext [MSGSIZE]byte
}

func main() {
    // Create or open the message queue
    msqid, _, errno := syscall.Syscall(syscall.SYS_MSGGET, uintptr(MSGKEY), uintptr(IPC_CREAT|0666), 0) //IPC_CREAT|0666 -> create queue if it doesn't exist and set perm
    if errno != 0 {
        fmt.Fprintf(os.Stderr, "Error creating/opening message queue: %v\n", errno)
        os.Exit(1)
    }

    // Send a message to the queue
    message := "Hello from the producer!"
    var msg Msgbuf
    msg.Mtype = 1
    copy(msg.Mtext[:], message)

    _, _, errno = syscall.Syscall6(syscall.SYS_MSGSND, msqid, uintptr(unsafe.Pointer(&msg)), uintptr(len(message)), 0, 0, 0)
    if errno != 0 {
        fmt.Fprintf(os.Stderr, "Error sending message to queue: %v\n", errno)
        os.Exit(1)
    }

    fmt.Println("Message sent to the queue by the producer")
}
