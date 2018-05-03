#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstring>
#include<string>
#include <unistd.h>

#include "err.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#define BUFFER_SIZE   2000
#define QUEUE_LENGTH     5
//#define PORT_NUM     10004

enum ClientState { a, b, end, b1, b2, back };
enum Signal { up, down, enter, trash};
enum Action {nothing, disconnect, A, B1, B2};

Signal what_signal (const char* buf, int len) {
    if (len < 2 || len > 3) return trash;
    if (len == 2 && buf[0] == 13 && buf[1] == 0) return enter;
    if (len == 3 && buf[0] == 27 && buf[1] == 91) {
        if (buf[2] == 65) return up;
        if (buf[2] == 66) return down;
    }
    return trash;
}
void print_space(int msg_sock, ClientState state, int i) {
    if (state == i) {
        write(msg_sock, "*", 1);
    } else {
        write(msg_sock, " ", 1);
    }
}
void print_menu(int msg_sock, ClientState state) {
    write(msg_sock, "\033[2J",4); //clear
    write(msg_sock, "\033[H",3);
    if (state < 3) {
        print_space(msg_sock, state, 0);
        write(msg_sock, "Opcja A\n", 8);
        write(msg_sock, "\033[9D", 4);
        print_space(msg_sock, state, 1);
        write(msg_sock, "Opcja B\n", 8);
        write(msg_sock, "\033[9D", 4);
        print_space(msg_sock, state, 2);
        write(msg_sock, "Koniec\n", 7);
        write(msg_sock, "\033[7D", 4);
    } else {
        print_space(msg_sock, state, 3);
        write(msg_sock, "Opcja B1\n", 9);
        write(msg_sock, "\033[10D", 5);
        print_space(msg_sock, state, 4);
        write(msg_sock, "Opcja B2\n", 9);
        write(msg_sock, "\033[10D", 5);
        print_space(msg_sock, state, 5);
        write(msg_sock, "Wstecz\n", 7);
        write(msg_sock, "\033[7D", 4);
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
                //*mssg = "B1";
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
    int PORT_NUM = atoi(argv[1]);
    //char *clearmagic = "\033[2J\033H";
    int sock, msg_sock;
    struct sockaddr_in server_address{};
    struct sockaddr_in client_address{};
    socklen_t client_address_len;
    //std::string buffer;
    char buffer[BUFFER_SIZE];
    ssize_t len, snd_len;

    sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (sock < 0)
        syserr("socket");
    // after socket() call; we should close(sock) on any execution path;
    // since all execution paths exit immediately, sock would be closed when program terminates

    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(PORT_NUM); // listening on port PORT_NUM

    // bind the socket to a concrete address
    if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        syserr("bind");

    // switch to listening (passive open)
    if (listen(sock, QUEUE_LENGTH) < 0)
        syserr("listen");

    printf("accepting client connections on port %hu\n", ntohs(server_address.sin_port));
    for (;;) {
        ClientState client = a;
        Action action;
        client_address_len = sizeof(client_address);
        // get client connection from the socket
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0)
            syserr("accept");
        write(msg_sock,"\377\375\042\377\373\001",6);
        print_menu(msg_sock, client);

        do {
            char mssg[3];
            action = nothing;
            //read from socket
            len = read(msg_sock, buffer, sizeof(buffer));
            if (len < 0)
                syserr("reading from client socket");
            else {
                printf("the message was %d\n", what_signal(buffer, len));
                change_state(what_signal(buffer, len), &client, &action);
                printf("now the client looks like %d\n", client);
                print_menu(msg_sock, client);
                printf("now we will have to do %d\n", action);
                //printf("and the thing to send is %s\n", mssg);
                switch (action) {
                    case A:{
                        std::string aux_mssg = "A";
                        std::strcpy(mssg, aux_mssg.c_str());
                        snd_len = write(msg_sock, mssg, 1);
                        printf("we send %d\n", snd_len);
                        //if (snd_len != 1);
                        //    syserr("writing to client socket");
                        break;
                    }
                    case B1: {
                        auto mssg = const_cast<char *>("B1");
                        snd_len = write(msg_sock, mssg, 2);
                        printf("we send %d\n", snd_len);
                        //if (snd_len != 2);
                        //    syserr("writing to client socket");
                        break;

                    }
                    case B2: {
                        auto mssg = const_cast<char *>("B2");
                        printf("we send %d\n", snd_len);
                        snd_len = write(msg_sock, mssg, 2);
                        //if (snd_len != 2);
                        //    syserr("writing to client socket");
                        break;
                    }

                    case disconnect:{
                        len = -1;
                        break;
                    }
                    case nothing: {
                        break;
                    }
                }

                //printf("read from socket: %zd bytes: %.*s\n", len, (int) len, buffer);
                /*for (int iter = 0; iter < len; iter++) {
                    printf("%d\n", buffer[iter]);
                }*/
                //write to socket
                /*snd_len = write(msg_sock, buffer, len);
                if (snd_len != len)
                    syserr("writing to client socket");*/
            }
        } while (len > 0);
        printf("ending connection\n");
        if (close(msg_sock) < 0)
            syserr("close");
    }

    return 0;
}

#pragma clang diagnostic pop