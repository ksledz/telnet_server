#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <utility>

#include "err.h"

#define BUFFER_SIZE   2000
#define QUEUE_LENGTH 5
#define SINGLE_KEY_MODE  "\377\375\042\377\373\001"
#define CLEAR_SCREEN  "\033[2J\033[H"
#define BACK1  "\033["
#define BACK2  "D"


enum Signal {
    up, down, enter, trash
};


std::string print_with_enter(const std::string &s) {
    std::stringstream ss;
    ss << s << "\n" << BACK1 << (s.size() + 2) << BACK2;
    return ss.str();
}

class TCPSocket {
public:
    int sock, msg_sock;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_len;
    char buffer[BUFFER_SIZE];
    ssize_t len;
    bool connected;

    TCPSocket(uint16_t PORT_NUM) {
        connected = false;
        sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
        if (sock < 0)
            syserr("socket");
        server_address.sin_family = AF_INET; // IPv4
        server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
        server_address.sin_port = htons(PORT_NUM); // listening on port PORT_NUM
        if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
            syserr("bind");
        if (listen(sock, QUEUE_LENGTH) < 0)
            syserr("listen");
        printf("accepting client connections on port %hu\n", ntohs(server_address.sin_port));
    }

    void accept_connection() {
        connected = true;
        client_address_len = sizeof(client_address);
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0)
            syserr("accept");
    }

    void end_connection() {
        printf("ending connection\n");
        if (close(msg_sock) < 0) syserr("close");
        connected = false;
    }

    void send(const std::string &message) {
        write(msg_sock, message.c_str(), message.size());
    }

    void receive() {
        len = read(msg_sock, buffer, sizeof(buffer));
        if (len < 0)
            syserr("reading from client socket");
    }

    Signal receive_signal() {
        receive();
        if (len < 2 || len > 3) return trash;
        if (len == 2 && buffer[0] == 13 && buffer[1] == 0) return enter;
        if (len == 3 && buffer[0] == 27 && buffer[1] == 91) {
            if (buffer[2] == 65) return up;
            if (buffer[2] == 66) return down;
        }
        return trash;
    }

    ~TCPSocket() {
        if (close(sock) < 0) syserr("close");
    }
};


class MenuOption {
    int move;
    std::string printed;
    bool disconnect;
public:
    std::string name;

    int react_to_enter(TCPSocket *sock) {
        if (!printed.empty()) sock->send(printed);
        if (disconnect) sock->end_connection();
        return move;

    }

    MenuOption(std::string name, std::string printed, int move = 0, bool disconnect = false) :
            move(move),
            printed(std::move(printed)),
            disconnect(disconnect),
            name(std::move(name)){}

};

class UI {
    std::vector<std::vector<MenuOption>> screens;
    int menu;
    int option;
    TCPSocket *sock;
public:
    UI(unsigned long screen_number, TCPSocket *sock) :
            sock(sock),
            menu(0),
            option(0) {
        screens.resize(screen_number);
    }

    int add_option(unsigned long screen_number, const MenuOption &option) {
        if (screen_number > screens.size()) return 1;
        screens[screen_number].push_back(option);
        return 0;
    }

    void reset_menu() {
        menu = 0;
        option = 0;
    }

    void print_menu() {
        sock->send(CLEAR_SCREEN);
        int i = 0;
        for (auto moption: screens[menu]) {
            sock->send((i == option) ? "*" : " ");
            sock->send(print_with_enter(moption.name));
            i++;
        }
    }

    void react_to_signal(Signal signal) {
        switch (signal) {
            case down: {
                option++;
                if (option == screens[menu].size()) option = 0;
                print_menu();
                break;
            }
            case up: {
                option--;
                if (option == -1) option += screens[menu].size();
                print_menu();
                break;
            }
            case enter: {
                int move = screens[menu][option].react_to_enter(sock);
                if (move != 0) {
                    menu += (move + screens.size());
                    menu %= screens.size();
                    option = 0;
                    print_menu();
                }
                break;
            }
            case trash:
                break;

        }

    }
};


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fatal("Usage: %s host port message ...\n", argv[0]);
    }
    char *p_end;

    auto PORT_NUM = static_cast<uint16_t>(strtol(argv[1], &p_end, 10));
    if (*p_end != '\0') syserr("bad argument format");
    TCPSocket tcpsocket(PORT_NUM);
    UI ui(2, &tcpsocket);
    ui.add_option(0, MenuOption("Opcja A", "A"));
    ui.add_option(0, MenuOption("Opcja B", "", 1));
    ui.add_option(0, MenuOption("Koniec", "", 0, true));
    ui.add_option(1, MenuOption("Opcja B1", "B1"));
    ui.add_option(1, MenuOption("Opcja B2", "B2"));
    ui.add_option(1, MenuOption("Wstecz", "", -1));

    for (;;) {

        tcpsocket.accept_connection();
        tcpsocket.send(SINGLE_KEY_MODE);
        ui.reset_menu();
        ui.print_menu();

        do {
            ui.react_to_signal(tcpsocket.receive_signal());
        } while (tcpsocket.connected);
    }
    return 0;
}


