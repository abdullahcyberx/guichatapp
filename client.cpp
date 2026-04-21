/*
 * client.cpp  –  GUI Chat Client (C++ / GTK-3)
 *
 * Usage:  ./client <server_ip> <port> <name>
 *
 * Layout:
 *   ┌─────────────────────────────────────┐
 *   │  Chat App  –  logged in as <name>   │  ← title bar
 *   ├──────────────────────┬──────────────┤
 *   │                      │ Online Users │
 *   │   Chat area          │ ──────────── │
 *   │   (scrolled)         │  • alice     │
 *   │                      │  • bob       │
 *   ├──────────────────────┴──────────────┤
 *   │  [  message input box  ] [ Send ]   │
 *   │  [ /list ] [ /quit ]                │
 *   └─────────────────────────────────────┘
 *
 * Threading:
 *   - GTK runs on the main thread (required by GTK)
 *   - A background recv_thread calls g_idle_add() to
 *     push incoming text safely onto the GTK main loop
 */

#include <gtk/gtk.h>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 2048

/* ── globals ─────────────────────────────────────────────────────────── */
static int          sock_fd  = -1;
static volatile int running  = 1;
static std::string  my_name;

/* GTK widgets we need from multiple functions */
static GtkWidget *chat_view   = nullptr;   /* GtkTextView  */
static GtkWidget *entry       = nullptr;   /* GtkEntry     */
static GtkWidget *users_view  = nullptr;   /* GtkTextView  */
static GtkTextBuffer *chat_buf  = nullptr;
static GtkTextBuffer *users_buf = nullptr;

/* ── helpers ─────────────────────────────────────────────────────────── */
static void send_msg(const std::string &msg)
{
    if (sock_fd >= 0)
        send(sock_fd, msg.c_str(), msg.size(), 0);
}

/* Append text to the chat window — must be called on GTK main thread */
static void append_chat(const std::string &text)
{
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(chat_buf, &end);
    gtk_text_buffer_insert(chat_buf, &end, text.c_str(), -1);

    /* Auto-scroll to bottom */
    GtkTextMark *mark = gtk_text_buffer_get_insert(chat_buf);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(chat_view), mark);
}

/* Update the online-users panel */
static void update_users(const std::string &list_text)
{
    gtk_text_buffer_set_text(users_buf, list_text.c_str(), -1);
}

/* ── idle callbacks (called on GTK main thread via g_idle_add) ───────── */
struct IdleData { std::string text; };

static gboolean idle_append_chat(gpointer data)
{
    IdleData *d = static_cast<IdleData *>(data);
    append_chat(d->text);
    delete d;
    return G_SOURCE_REMOVE;
}

static gboolean idle_update_users(gpointer data)
{
    IdleData *d = static_cast<IdleData *>(data);
    update_users(d->text);
    delete d;
    return G_SOURCE_REMOVE;
}

static gboolean idle_quit(gpointer /*data*/)
{
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

/* ── background receive thread ───────────────────────────────────────── */
static void *recv_thread(void * /*arg*/)
{
    char buf[BUFFER_SIZE];
    ssize_t n;

    while (running && (n = recv(sock_fd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        std::string msg(buf);

        /* Detect /list response: starts with "Connected clients:" */
        if (msg.rfind("Connected clients:", 0) == 0) {
            /* Show in users panel */
            g_idle_add(idle_update_users, new IdleData{msg});
            /* Also show in chat */
            g_idle_add(idle_append_chat,  new IdleData{"[server] " + msg + "\n"});
        } else {
            g_idle_add(idle_append_chat, new IdleData{msg});
        }
    }

    if (running) {
        g_idle_add(idle_append_chat,
            new IdleData{"[client] Disconnected from server.\n"});
        g_idle_add(idle_quit, nullptr);
    }
    return nullptr;
}

/* ── GTK signal handlers ─────────────────────────────────────────────── */

/* Send whatever is in the entry box */
static void on_send(GtkWidget * /*widget*/, gpointer /*data*/)
{
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!text || strlen(text) == 0) return;

    std::string msg(text);
    gtk_entry_set_text(GTK_ENTRY(entry), "");   /* clear input */

    /* Show locally in chat */
    append_chat("[you]: " + msg + "\n");

    send_msg(msg);

    if (msg == "/quit") {
        running = 0;
        gtk_main_quit();
    }
}

/* Enter key in the text field also sends */
static void on_entry_activate(GtkWidget *widget, gpointer data)
{
    on_send(widget, data);
}

/* /list button */
static void on_list(GtkWidget * /*w*/, gpointer /*d*/)
{
    append_chat("[you]: /list\n");
    send_msg("/list");
}

/* /quit button */
static void on_quit_btn(GtkWidget * /*w*/, gpointer /*d*/)
{
    append_chat("[you]: /quit\n");
    send_msg("/quit");
    running = 0;
    gtk_main_quit();
}

/* Window X button */
static gboolean on_window_delete(GtkWidget * /*w*/, GdkEvent * /*e*/,
                                  gpointer /*d*/)
{
    send_msg("/quit");
    running = 0;
    gtk_main_quit();
    return FALSE;
}

/* ── build GUI ───────────────────────────────────────────────────────── */
static void build_gui(int *argc, char ***argv)
{
    gtk_init(argc, argv);

    /* Main window */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window),
        ("Chat App  –  logged in as " + my_name).c_str());
    gtk_window_set_default_size(GTK_WINDOW(window), 780, 520);
    gtk_container_set_border_width(GTK_CONTAINER(window), 8);
    g_signal_connect(window, "delete-event", G_CALLBACK(on_window_delete), nullptr);

    /* Outer vertical box */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* ── Top: horizontal paned (chat | users) ── */
    GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
    gtk_paned_set_position(GTK_PANED(hpaned), 560);

    /* Chat area */
    GtkWidget *chat_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(chat_scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    chat_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(chat_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(chat_view), 6);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(chat_view), 6);
    chat_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_view));
    gtk_container_add(GTK_CONTAINER(chat_scroll), chat_view);
    gtk_paned_pack1(GTK_PANED(hpaned), chat_scroll, TRUE, TRUE);

    /* Users panel */
    GtkWidget *users_frame = gtk_frame_new("Online Users");
    GtkWidget *users_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(users_scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    users_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(users_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(users_view), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(users_view), 6);
    users_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(users_view));
    gtk_container_add(GTK_CONTAINER(users_scroll), users_view);
    gtk_container_add(GTK_CONTAINER(users_frame), users_scroll);
    gtk_paned_pack2(GTK_PANED(hpaned), users_frame, FALSE, FALSE);

    /* ── Bottom: input row ── */
    GtkWidget *input_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), input_hbox, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry),
        "Type a message or /msg <name> <text> ...");
    gtk_box_pack_start(GTK_BOX(input_hbox), entry, TRUE, TRUE, 0);
    g_signal_connect(entry, "activate", G_CALLBACK(on_entry_activate), nullptr);

    GtkWidget *send_btn = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(input_hbox), send_btn, FALSE, FALSE, 0);
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send), nullptr);

    /* ── Bottom: command buttons row ── */
    GtkWidget *btn_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), btn_hbox, FALSE, FALSE, 0);

    GtkWidget *list_btn = gtk_button_new_with_label("/list  –  refresh online users");
    gtk_box_pack_start(GTK_BOX(btn_hbox), list_btn, FALSE, FALSE, 0);
    g_signal_connect(list_btn, "clicked", G_CALLBACK(on_list), nullptr);

    GtkWidget *quit_btn = gtk_button_new_with_label("/quit  –  disconnect");
    gtk_box_pack_start(GTK_BOX(btn_hbox), quit_btn, FALSE, FALSE, 0);
    g_signal_connect(quit_btn, "clicked", G_CALLBACK(on_quit_btn), nullptr);

    gtk_widget_show_all(window);
}

/* ── main ────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <server_ip> <port> <name>\n";
        return EXIT_FAILURE;
    }

    const std::string server_ip = argv[1];
    const int         port      = std::atoi(argv[2]);
    my_name                     = argv[3];

    if (port <= 0) {
        std::cerr << "Invalid port number.\n";
        return EXIT_FAILURE;
    }

    /* Connect to server */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, server_ip.c_str(), &saddr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << server_ip << "\n";
        close(sock_fd); return EXIT_FAILURE;
    }
    if (connect(sock_fd,
                reinterpret_cast<sockaddr *>(&saddr),
                sizeof(saddr)) < 0) {
        std::cerr << "Could not connect to " << server_ip
                  << ":" << port << " – is the server running?\n";
        close(sock_fd); return EXIT_FAILURE;
    }

    /* Send name */
    send(sock_fd, my_name.c_str(), my_name.size(), 0);

    /* Build and show GUI */
    build_gui(&argc, &argv);

    /* Start background receive thread */
    pthread_t tid;
    pthread_create(&tid, nullptr, recv_thread, nullptr);

    /* GTK main loop (blocks until gtk_main_quit()) */
    gtk_main();

    /* Cleanup */
    running = 0;
    shutdown(sock_fd, SHUT_RDWR);
    pthread_join(tid, nullptr);
    close(sock_fd);

    return 0;
}
