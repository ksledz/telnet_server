#include <utility>

#include <utility>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>
#include <sstream>

#include "err.h"

#define BUFFER_SIZE   2000
#define QUEUE_LENGTH 5


std::string clear_menu = "\377\375\042\377\373\001";
std::string back_1 = "\033[";
std::string back_2 = "D";

enum ClientState {
    a, b, end, b1, b2, back
};
enum Signal {
    up, down, enter, trash
};
enum Action {
    nothing, disconnect, A, B1, B2
};

std::string print_with_enter(const std::string &s) {
    std::stringstream ss;
    ss << s << "\n" << back_1 << (s.size() + 2) << back_2;
    return ss.str();
}

class TCPSocket {
public:
    int sock, msg_sock;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_len;
    char buffer[BUFFER_SIZE];
    ssize_t len, snd_len;
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
    TCPSocket *sock;
public:
    std::string name;

    int react_to_enter() {
        if (!printed.empty()) sock->send(printed);
        if (disconnect) sock->end_connection();
        return move;

    }
    MenuOption (std::string name, int move = 0, std::string printed = "", bool disconnect = false, TCPSocket* sock = NULL) :
    move(move),
    printed(std::move(printed)),
    disconnect(disconnect),
    name(std::move(name)),
    sock(sock) {}



};

class UI {
    std::vector<std::vector<MenuOption>> screens;
    int menu;
    int option;
    TCPSocket *sock;
public:
    UI (unsigned long screen_number, TCPSocket* sock) :
    sock(sock),
    menu(0),
    option(0)
    {
        screens.resize(screen_number);
    }

    int add_option(unsigned long screen_number, const MenuOption &option) {
        if (screen_number > screens.size()) return 1;
        screens[screen_number].push_back(option);
    }

    void reset_menu() {
        menu = 0;
        option = 0;
    }
    void print_menu() {
        //sock->send(clear_menu);
        sock->send("\033[2J");
        sock->send("\033[H");
        int i = 0;
        for (auto moption: screens[menu]) {
            if (i == option) {
                sock->send("*");
            } else {
                sock->send(" ");
            }
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
                int move = screens[menu][option].react_to_enter();
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


Signal what_signal(const char *buf, ssize_t len) {
    if (len < 2 || len > 3) return trash;
    if (len == 2 && buf[0] == 13 && buf[1] == 0) return enter;
    if (len == 3 && buf[0] == 27 && buf[1] == 91) {
        if (buf[2] == 65) return up;
        if (buf[2] == 66) return down;
    }
    return trash;
}

void print_space(TCPSocket *sock, ClientState state, int i) {
    if (state == i) {
        sock->send("*");
    } else {
        sock->send(" ");
    }
}

void print_menu(TCPSocket *msg_sock, ClientState state) {

    msg_sock->send("\033[2J");
    msg_sock->send("\033[H");
    if (state < 3) {
        print_space(msg_sock, state, 0);
        msg_sock->send(print_with_enter("Opcja A"));
        print_space(msg_sock, state, 1);
        msg_sock->send(print_with_enter("Opcja B"));
        print_space(msg_sock, state, 2);
        msg_sock->send(print_with_enter("Koniec"));

    } else {
        print_space(msg_sock, state, 3);
        msg_sock->send(print_with_enter("Opcja B1"));
        print_space(msg_sock, state, 4);
        msg_sock->send(print_with_enter("Opcja B2"));
        print_space(msg_sock, state, 5);
        msg_sock->send(print_with_enter("Wstecz"));
    }

}

void change_state(Signal signal, ClientState *state, Action *action) {
    int temp_s = *state;
    bool menu2 = false;
    if (*state >= 3) menu2 = true;

    if (signal == down) {
        temp_s++;
    }
    if (signal == up) {
        temp_s += 2;
    }
    if (signal < 2) {
        temp_s %= 3;
        if (menu2) temp_s += 3;
    }
    (*state) = static_cast<ClientState>(temp_s);
    if (signal == enter) {
        switch (*state) {
            case a: {
                *action = A;
                break;
            }
            case b: {
                *state = b1;
                break;
            }
            case end: {
                *action = disconnect;
                break;
            }
            case b1: {
                *action = B1;
                break;
            }
            case b2: {
                *action = B2;
                break;
            }
            case back: {
                *state = a;
                break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fatal("Usage: %s host port message ...\n", argv[0]);
    }
    char *p_end;

    auto PORT_NUM = static_cast<uint16_t>(strtol(argv[1], &p_end, 10));
    if (*p_end != '\0') syserr("bad argument");
    TCPSocket tcpsocket(PORT_NUM);
    UI ui(2, &tcpsocket);
    ui.add_option(0, MenuOption("Opcja A", 0, "A", false, &tcpsocket));
    ui.add_option(0, MenuOption("Opcja B", 1, "", false, &tcpsocket));
    ui.add_option(0, MenuOption("Koniec", 0, "", true, &tcpsocket));
    ui.add_option(1, MenuOption("Opcja B1", 0, "B1", false, &tcpsocket));
    ui.add_option(1, MenuOption("Opcja B2", 0, "B2", false, &tcpsocket));
    ui.add_option(1, MenuOption("Wstecz", -1, "", false, &tcpsocket));

    for (;;) {
        ClientState client = a;
        Action action;
        tcpsocket.accept_connection();
        tcpsocket.send(clear_menu);
        //print_menu(&tcpsocket, client);
        ui.reset_menu();
        ui.print_menu();

        do {
            ui.react_to_signal(tcpsocket.receive_signal());
            /*action = nothing;
            //tcpsocket.receive();
            change_state(tcpsocket.receive_signal(), &client, &action);

            print_menu(&tcpsocket, client);

            switch (action) {
                case A: {
                    tcpsocket.send("A");
                    break;
                }
                case B1: {
                    tcpsocket.send("B1");
                    break;
                }
                case B2: {
                    tcpsocket.send("B2");
                    break;
                }
                case disconnect: {
                    tcpsocket.len = -1;
                    break;
                }
                case nothing: {
                    break;
                }
            }*/

        } while (tcpsocket.connected);
        //tcpsocket.end_connection();
    }
    return 0;
}


