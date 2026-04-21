/*
 * server.cpp - Multi-threaded Chat Server (C++)
 *
 * Usage: ./server <port>
 *
 * Features:
 *  - Accepts multiple clients, each handled in a separate thread
 *  - Maintains a thread-safe linked list of connected clients
 *  - Supports /list, /msg, /quit commands
 *  - Rejects duplicate client names
 *  - Server logs activity only — message content is never printed
 */

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <csignal>

#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_NAME_LEN  64
#define BUFFER_SIZE   1024

struct ClientNode {
    char        name[MAX_NAME_LEN];
    int         fd;
    pthread_t   tid;
    ClientNode *next;
    ClientNode() : fd(-1), tid(0), next(nullptr) { name[0] = '\0'; }
};

static ClientNode      *client_list = nullptr;
static pthread_mutex_t  list_mutex  = PTHREAD_MUTEX_INITIALIZER;

static void list_add(ClientNode *node) {
    node->next  = client_list;
    client_list = node;
}

static void list_remove(int fd) {
    ClientNode **pp = &client_list;
    while (*pp) {
        if ((*pp)->fd == fd) {
            ClientNode *tmp = *pp;
            *pp = tmp->next;
            delete tmp;
            return;
        }
        pp = &(*pp)->next;
    }
}

static ClientNode *list_find_name(const std::string &name) {
    for (ClientNode *n = client_list; n; n = n->next)
        if (name == n->name) return n;
    return nullptr;
}

static void send_msg(int fd, const std::string &msg) {
    send(fd, msg.c_str(), msg.size(), 0);
}

static void cmd_list(int requester_fd) {
    std::string response = "Connected clients:\n";
    pthread_mutex_lock(&list_mutex);
    for (ClientNode *n = client_list; n; n = n->next)
        response += "  " + std::string(n->name) + "\n";
    pthread_mutex_unlock(&list_mutex);
    send_msg(requester_fd, response);
}

static void cmd_msg(int sender_fd, const std::string &sender_name,
                    const std::string &args) {
    size_t space = args.find(' ');
    if (space == std::string::npos || space == 0) {
        send_msg(sender_fd, "ERROR: /msg requires a destination name and message.\n");
        return;
    }
    std::string dest_name = args.substr(0, space);
    std::string text      = args.substr(space + 1);
    if (text.empty()) {
        send_msg(sender_fd, "ERROR: /msg requires a message body.\n");
        return;
    }
    std::string out = "[" + sender_name + " -> you]: " + text + "\n";
    pthread_mutex_lock(&list_mutex);
    ClientNode *dest = list_find_name(dest_name);
    if (!dest) {
        pthread_mutex_unlock(&list_mutex);
        send_msg(sender_fd, "ERROR: Client '" + dest_name + "' not found.\n");
        return;
    }
    int dest_fd = dest->fd;
    pthread_mutex_unlock(&list_mutex);
    send_msg(dest_fd, out);
    send_msg(sender_fd, "Message sent to " + dest_name + ".\n");
}

static void *client_thread(void *arg) {
    ClientNode *me = static_cast<ClientNode *>(arg);
    char buf[BUFFER_SIZE];
    ssize_t n;

    std::cout << "[server] Thread started for client '"
              << me->name << "' (fd=" << me->fd << ")\n";

    while ((n = recv(me->fd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';
        std::string msg(buf);

        if (!msg.empty() && msg[0] == '/') {
            if (msg == "/list") {
                /* Log command type only */
                std::cout << "[server] " << me->name << " requested client list.\n";
                cmd_list(me->fd);

            } else if (msg.substr(0, 5) == "/msg ") {
                /* Log sender and destination only — message body stays private */
                size_t sp = msg.find(' ', 5);
                std::string dest = (sp != std::string::npos)
                                   ? msg.substr(5, sp - 5) : msg.substr(5);
                std::cout << "[server] " << me->name
                          << " sent a message to " << dest << ".\n";
                cmd_msg(me->fd, me->name, msg.substr(5));

            } else if (msg == "/quit") {
                std::cout << "[server] " << me->name << " requested disconnect.\n";
                send_msg(me->fd, "Goodbye!\n");
                break;

            } else {
                std::cout << "[server] " << me->name << " sent an unknown command.\n";
                send_msg(me->fd, "ERROR: Unknown command.\n");
            }
        } else {
            /* Plain text — log activity only, never the content */
            std::cout << "[server] " << me->name << " sent a message.\n";
            send_msg(me->fd, "[" + std::string(me->name) + "]: " + msg + "\n");
        }
    }

    std::cout << "[server] Client '" << me->name << "' disconnected.\n";
    int fd = me->fd;
    close(fd);
    pthread_mutex_lock(&list_mutex);
    list_remove(fd);
    pthread_mutex_unlock(&list_mutex);
    pthread_detach(pthread_self());
    return nullptr;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return EXIT_FAILURE;
    }
    int port = std::atoi(argv[1]);
    if (port <= 0) { std::cerr << "Invalid port number.\n"; return EXIT_FAILURE; }

    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind"); return EXIT_FAILURE;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen"); return EXIT_FAILURE;
    }

    std::cout << "[server] Listening on port " << port << " ...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t clen = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               reinterpret_cast<sockaddr *>(&client_addr), &clen);
        if (client_fd < 0) { perror("accept"); continue; }

        std::cout << "[server] New connection from "
                  << inet_ntoa(client_addr.sin_addr) << "\n";

        char name_buf[MAX_NAME_LEN] = {};
        ssize_t nr = recv(client_fd, name_buf, sizeof(name_buf) - 1, 0);
        if (nr <= 0) { close(client_fd); continue; }
        name_buf[strcspn(name_buf, "\r\n")] = '\0';
        std::string name(name_buf);

        pthread_mutex_lock(&list_mutex);
        if (list_find_name(name)) {
            pthread_mutex_unlock(&list_mutex);
            send_msg(client_fd, "ERROR: Name already in use. Connection refused.\n");
            close(client_fd);
            std::cout << "[server] Rejected duplicate name '" << name << "'\n";
            continue;
        }
        ClientNode *node = new ClientNode();
        strncpy(node->name, name.c_str(), MAX_NAME_LEN - 1);
        node->fd = client_fd;
        list_add(node);
        pthread_mutex_unlock(&list_mutex);

        std::cout << "[server] Client '" << name << "' registered.\n";
        send_msg(client_fd,
            "Welcome, " + name + "! Type /list, /msg <n> <text>, or /quit.\n");

        if (pthread_create(&node->tid, nullptr, client_thread, node) != 0) {
            perror("pthread_create");
            pthread_mutex_lock(&list_mutex);
            list_remove(client_fd);
            pthread_mutex_unlock(&list_mutex);
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
