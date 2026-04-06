# Client-Server Messaging System

A TCP-based multi-client messaging system built in C/C++ that enables real-time text communication between clients through a centralized server.

---

## Overview

This project implements a scalable client-server architecture using low-level socket programming.  
The server handles multiple concurrent client connections, manages communication, and ensures reliable message delivery.

---

## How It Works

### Server
- Listens for incoming TCP connections
- Maintains a list of active clients and their states
- Routes messages (direct, broadcast)
- Buffers messages for offline clients
- Handles commands such as `LOGIN`, `SEND`, `BROADCAST`, `BLOCK`, etc.

### Client
- Connects to the server using IP + port
- Sends commands via a structured protocol
- Receives and displays messages in real time

---

## Features

- Multi-client support using `select()` for I/O multiplexing
- Client login/logout system with active client tracking
- Direct messaging between clients
- Broadcast messaging to all connected clients
- Block / unblock functionality for selective communication
- Offline message buffering and delivery upon reconnection
- Real-time client list synchronization

---

## Tech Stack

- **Language:** C/C++
- **Networking:** TCP sockets (POSIX)
- **Concurrency Model:** `select()`-based multiplexing
- **Build System:** Makefile

---


## Directory Structure

This repository follows the structure outlined below:

```
.
├── README.md
├── assignment1_package.sh
├── grader
│   ├── grader.cfg
│   └── grader_controller
└── pa1
    ├── Makefile
    ├── include
    │   ├── global.h
    │   └── logger.h
    ├── logs
    ├── object
    └── src
        ├── assignment1.cpp
        └── logger.cpp
```

## Environment

This project was developed and tested on a Linux-based environment (university servers).  
It relies on POSIX socket APIs and is expected to run on Unix-like systems (Linux/macOS).

## Context

This project was developed as part of a university systems programming / networking course.  
Most of the core design and implementation were completed independently.
