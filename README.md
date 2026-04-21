# [cite_start]GUI Chat Application (C++ / GTK-3) [cite: 3]

## Project Overview
[cite_start]This repository contains the source code for a GUI Chat Application developed for the CS-232 Operating Systems course[cite: 1]. [cite_start]It extends Assignment 02 by adding a graphical user interface to the chat client[cite: 8]. [cite_start]The underlying client/server architecture, socket programming, POSIX threading, and mutex synchronization remain the same[cite: 9]. 

[cite_start]**Authors:** Muhammad Abdullah (64168) & Muhammad Faizan Jamil (63539) [cite: 4]
[cite_start]**Language:** C++17 [cite: 4]

---

## Key Features
* [cite_start]**Graphical Interface:** The client presents a GTK-3 window providing a scrollable chat area, an online users panel, a message input field, command buttons, and full keyboard support[cite: 10].
* [cite_start]**Check Online Users:** The `/list` command fetches all connected client names from the server and displays them in the Online Users panel on the right side of the window[cite: 12].
* [cite_start]**Private Messaging:** The `/msg <n> <text>` command sends a private message to the named client[cite: 12].
* [cite_start]**Clean Disconnect:** The `/quit` command disconnects cleanly from the server and closes the GUI window[cite: 12].
* [cite_start]**Privacy-focused Server Logging:** The server logs only metadata—who sent a message and to whom—ensuring user privacy, and message content is never printed[cite: 39, 73].

---

## Architecture & Technologies
* [cite_start]**Socket Programming:** TCP socket operations like `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, and `recv()` provide reliable byte-stream communication between the server and clients[cite: 57].
* [cite_start]**Multi-threading:** * **Server:** Uses `pthread_create()` to spawn one thread per client on the server[cite: 57].
    * [cite_start]**Client:** The client uses two threads: the main thread for the GTK event loop and a background recv thread[cite: 47, 48, 49].
* [cite_start]**Concurrency Control:** The server keeps a singly linked list of ClientNode structs shared across all client threads[cite: 52]. [cite_start]A single `pthread_mutex_t` guards the list, preventing race conditions and data corruption[cite: 53].
* [cite_start]**GTK-3 Thread Safety:** `g_idle_add()` is the GTK-safe mechanism for updating widgets from a background thread onto the main GTK loop[cite: 57].
* [cite_start]**Safe Memory Management:** Replaces raw char array buffer management with `std::string` for safe string handling, and uses `new` and `delete` to ensure every allocation has a matching deallocation with no memory leaks[cite: 21, 57].

---

## Getting Started

### Prerequisites
[cite_start]GTK-3 development libraries must be installed[cite: 524]. [cite_start]On Ubuntu / Kali / WSL: [cite: 524]
```bash
sudo apt-get install libgtk-3-dev
