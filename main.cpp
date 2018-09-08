#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

#include "err.h"

#define BUFFER_SIZE   2000
#define QUEUE_LENGTH 5



std::string clear_menu = "\377\375\042\377\373\001";



class TCPSocket {
public:
    int sock, msg_sock;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_len;
    char buffer[BUFFER_SIZE];
    ssize_t len, snd_len;

    TCPSocket (uint16_t PORT_NUM) {
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
        client_address_len = sizeof(client_address);
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0)
            syserr("accept");
    }
    void end_connection () {
        printf("ending connection\n");
        if (close(msg_sock) < 0) syserr("close");
    }
    void send (const std::string &message) {
        write(msg_sock, message.c_str(), message.size());
    }
    ~TCPSocket() {
        if (close(sock) < 0) syserr("close");
    }
};




enum ClientState { a, b, end, b1, b2, back };
enum Signal { up, down, enter, trash};
enum Action {nothing, disconnect, A, B1, B2};

Signal what_signal (const char* buf, ssize_t len) {
    if (len < 2 || len > 3) return trash;
    if (len == 2 && buf[0] == 13 && buf[1] == 0) return enter;
    if (len == 3 && buf[0] == 27 && buf[1] == 91) {
        if (buf[2] == 65) return up;
        if (buf[2] == 66) return down;
    }
    return trash;
}
void print_space(int sock, ClientState state, int i) {
    ssize_t snd_len;
    if (state == i) {
        //sock.send("*");
        snd_len = write(sock, "*", 1);
    } else {

        snd_len = write(sock, " ", 1);
    }
}
void print_menu(int msg_sock, ClientState state) {
    ssize_t snd_len;
    snd_len = write(msg_sock, "\033[2J",4); //clear
    snd_len = write(msg_sock, "\033[H",3);
    if (state < 3) {
        print_space(msg_sock, state, 0);
        snd_len = write(msg_sock, "Opcja A\n", 8);
        snd_len = write(msg_sock, "\033[9D", 4);
        print_space(msg_sock, state, 1);
        snd_len = write(msg_sock, "Opcja B\n", 8);
        snd_len = write(msg_sock, "\033[9D", 4);
        print_space(msg_sock, state, 2);
        snd_len = write(msg_sock, "Koniec\n", 7);
        snd_len = write(msg_sock, "\033[7D", 4);
    } else {
        print_space(msg_sock, state, 3);
        snd_len = write(msg_sock, "Opcja B1\n", 9);
        snd_len = write(msg_sock, "\033[10D", 5);
        print_space(msg_sock, state, 4);
        snd_len = write(msg_sock, "Opcja B2\n", 9);
        snd_len = write(msg_sock, "\033[10D", 5);
        print_space(msg_sock, state, 5);
        snd_len = write(msg_sock, "Wstecz\n", 7);
        snd_len = write(msg_sock, "\033[7D", 4);
    }

}
void change_state (Signal signal, ClientState* state, Action* action){
    int temp_s = *state;
    bool menu2 = false;
    if (*state >= 3) menu2 = true;

    if (signal == down) {
        temp_s++;
    }
    if (signal == up) {
        temp_s+=2;
    }
    if (signal < 2) {
        temp_s%=3;
        if (menu2) temp_s+=3;
    }
    (*state) = static_cast<ClientState>(temp_s);
    if (signal == enter) {
        switch(*state) {
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
                //*mssg = "B2";
                break;
            }
            case back: {
                *state = a;
                break;
            }
        }
    }
}
int main(int argc, char *argv[])
{
    if (argc != 2) {
        fatal("Usage: %s host port message ...\n", argv[0]);
    }
    char* p_end;

    auto PORT_NUM = static_cast<uint16_t>(strtol(argv[1], &p_end, 10));
    if (*p_end != '\0') syserr("bad argument");
    //char *clearmagic = "\033[2J\033H";
    TCPSocket tcpsocket(PORT_NUM);



    for (;;) {
        ClientState client = a;
        Action action;
        tcpsocket.accept_connection();
        tcpsocket.send(clear_menu);
        print_menu(tcpsocket.msg_sock, client);

        do {
            action = nothing;
            tcpsocket.len = read(tcpsocket.msg_sock, tcpsocket.buffer, sizeof(tcpsocket.buffer));
            if (tcpsocket.len < 0)
                syserr("reading from client socket");
            else {
                change_state(what_signal(tcpsocket.buffer, tcpsocket.len), &client, &action);

                print_menu(tcpsocket.msg_sock, client);

                switch (action) {
                    case A:{
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
                    case disconnect:{
                        tcpsocket.len = -1;
                        break;
                    }
                    case nothing: {
                        break;
                    }
                }
            }
        } while (tcpsocket.len > 0);
        tcpsocket.end_connection();
    }
    return 0;
}


