# GUI Chat Application (C++ / GTK-3)

## Project Overview
This repository contains the source code for a GUI Chat Application developed for the CS-232 Operating Systems course. It extends Assignment 02 by adding a graphical user interface to the chat client. The underlying client/server architecture, socket programming, POSIX threading, and mutex synchronization remain the same. 

**Authors:** Muhammad Abdullah (64168) & Muhammad Faizan Jamil (63539)
**Language:** C++17

---

## Key Features
* **Graphical Interface:** The client presents a GTK-3 window providing a scrollable chat area, an online users panel, a message input field, command buttons, and full keyboard support.
* **Check Online Users:** The `/list` command fetches all connected client names from the server and displays them in the Online Users panel on the right side of the window.
* **Private Messaging:** The `/msg <n> <text>` command sends a private message to the named client.
* **Clean Disconnect:** The `/quit` command disconnects cleanly from the server and closes the GUI window.
* **Privacy-focused Server Logging:** The server logs only metadata—who sent a message and to whom—ensuring user privacy, and message content is never printed.

---

## Architecture & Technologies
* **Socket Programming:** TCP socket operations like `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, and `recv()` provide reliable byte-stream communication between the server and clients.
* **Multi-threading:** * **Server:** Uses `pthread_create()` to spawn one thread per client on the server.
    * **Client:** The client uses two threads: the main thread for the GTK event loop and a background recv thread.
* **Concurrency Control:** The server keeps a singly linked list of ClientNode structs shared across all client threads. A single `pthread_mutex_t` guards the list, preventing race conditions and data corruption.
* **GTK-3 Thread Safety:** `g_idle_add()` is the GTK-safe mechanism for updating widgets from a background thread onto the main GTK loop.
* **Safe Memory Management:** Replaces raw char array buffer management with `std::string` for safe string handling, and uses `new` and `delete` to ensure every allocation has a matching deallocation with no memory leaks.

---

## Getting Started

### Prerequisites
GTK-3 development libraries must be installed. On Ubuntu / Kali / WSL:
```bash
sudo apt-get install libgtk-3-dev
