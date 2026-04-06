/**
*@author  Team Members jcibarra dabrober
* @version 1.0
*
* @section LICENSE
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details at
* http://www.gnu.org/copyleft/gpl.html
*
* @section DESCRIPTION
*
*/
#include <iostream>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <inttypes.h>

#include "../include/global.h"
#include "../include/logger.h"
#include "../include/server.h"

#define BACKLOG 10
#define STDIN_FD 0
#define CMD_SIZE 100


extern void print_success(const char* cmd);
extern void print_error(const char* cmd);
extern void print_end(const char* cmd);
extern void handle_author();
extern void handle_ip();
extern void handle_port();
extern std::string get_ip_address();

extern std::vector<ClientInfo> clients;
extern std::vector<BufferedMessage> buffered_messages;
extern uint16_t listen_port_number;
extern std::string my_ip;

static int listen_file_descriptor_number = -1;
static int currents_fd = -1;

extern int is_client_in_list_and_logged_in(const std::string& ip);

int get_client_file_descriptor(const char* ip) {
    int client_fd = -1;
    for (const auto& client : clients) {
        if (client.ip == ip) {
            client_fd = client.file_descriptor;
            break;
        }
    }
    return client_fd;
}

std::string get_client_hostname(const char* ip) {
    std::string client_hostname;
    for (const auto& client : clients) {
        if (client.ip == ip) {
            client_hostname = client.hostname;
            break;
        }
    }
    return client_hostname;
}

uint64_t get_client_peer_port(const char* ip) {
    uint64_t client_peer_port = 0;
    for (const auto& client : clients) {
        if (client.ip == ip) {
            client_peer_port = client.peer_port;
            break;
        }
    }
    return client_peer_port;
}

static void buffer_message(const char* sender_ip,
                           const char* receiver_ip,
                           const char* msg) {
    if (buffered_messages.size() >= 100) {
        return;
    }

    BufferedMessage bm;
    bm.sender_ip   = sender_ip;
    bm.receiver_ip = receiver_ip;
    bm.msg         = msg;
    buffered_messages.push_back(bm);
}

/* CHANGE HERE: send ALL known clients, not just logged-in ones */
static void send_client_list(int client_fd) {
    char response[BUFSIZ];
    memset(response, 0, sizeof(response));
    
    for (const auto& client : clients) {
        // Do NOT filter by client.logged_in here; we want the full known list
        char line[256];
        snprintf(line, sizeof(line), "%s %s %d\n",
                 client.hostname.c_str(),
                 client.ip.c_str(),
                 client.peer_port);
        strncat(response, line, sizeof(response) - strlen(response) - 1);
    }
    
    ssize_t sent = send(client_fd, response, strlen(response), 0);
    (void)sent;
}

/* Server-side LIST still shows only currently logged-in clients */
void handle_list_server() {
    print_success("LIST");

    std::vector<ClientInfo> sorted = clients;
    sort(sorted.begin(), sorted.end(),
         [](const ClientInfo& a, const ClientInfo& b) {
             return a.peer_port < b.peer_port;
         });

    int list_id = 1;

    for (const auto& c : sorted) {
        if (!c.logged_in) continue;  // only logged-in

        const char* hostname =
            (!c.hostname.empty()) ? c.hostname.c_str() : c.ip.c_str();

        cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",
                              list_id++,
                              hostname,
                              c.ip.c_str(),
                              c.peer_port);
    }

    print_end("LIST");
}

void handle_statistics_server() {
    print_success("STATISTICS");

    std::vector<ClientInfo> sorted = clients;
    sort(sorted.begin(), sorted.end(),
         [](const ClientInfo& a, const ClientInfo& b) {
             return a.peer_port < b.peer_port;
         });

    int list_id = 1;
    for (const auto& c : sorted) {
        const char* hostname =
            (!c.hostname.empty()) ? c.hostname.c_str() : c.ip.c_str();
        cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n",
                              list_id++,
                              hostname,
                              c.messages_sent,
                              c.messages_received,
                              c.logged_in ? "logged-in" : "logged-out");
    }

    print_end("STATISTICS");
}




static int make_listen_socket(const char* port_str) {
    int status;
    struct addrinfo hints;
    struct addrinfo* servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
        fprintf(stderr, "gia error: %s\n", gai_strerror(status));
        return -1;
    }

    int file_descriptor =
        socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (file_descriptor < 0) {
        perror("socket");
        freeaddrinfo(servinfo);
        return -1;
    }

    int optval = 1;
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) < 0) {
        perror("setsockopt");
    }

    if (::bind(file_descriptor, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind");
        close(file_descriptor);
        freeaddrinfo(servinfo);
        return -1;
    }

    if (listen(file_descriptor, BACKLOG) < 0) {
        perror("listen");
        close(file_descriptor);
        return -1;
    }

    listen_port_number = static_cast<uint16_t>(std::stoi(port_str));
    freeaddrinfo(servinfo);
    return file_descriptor;
}

static void accept_new_client(fd_set& current, int& max_fd) {
    sockaddr_in client_socket_info{};
    socklen_t   length = sizeof(client_socket_info);

    currents_fd =
        accept(listen_file_descriptor_number,
               (sockaddr*)&client_socket_info,
               &length);

    if (currents_fd < 0) {
        perror("accept");
        return;
    }

    FD_SET(currents_fd, &current);

    if (currents_fd > max_fd) {
        max_fd = currents_fd;
    }
}

bool send_payload_to_client(const char* reciever_ip, const char* payload) {
    int client_socket_fd = -1;

    for (const auto& client : clients) {
        if (client.ip == reciever_ip) {
            if (client.file_descriptor >= 0) {
                client_socket_fd = client.file_descriptor;
                break;
            } else {
                return false;
            }
        }
    }

    if (client_socket_fd < 0)
        return false;

    ssize_t sent = send(client_socket_fd, payload, strlen(payload), 0);
    return (sent > 0);
}

bool is_blocked(const char* sender_ip, const char* reciever_ip) {
    for (auto& client : clients) {
        if (client.ip == reciever_ip) {
            for (auto& blockedClients : client.blocked) {
                if (blockedClients == sender_ip) {
                    return true;
                }
            }
        }
    }
    return false;
}

void send_message(const char* sender_ip,
                  const char* reciever_ip,
                  const char* msg) {
    bool receiver_known  = false;
    bool receiver_online = false;

    for (const auto& client : clients) {
        if (client.ip == reciever_ip) {
            receiver_known = true;
            if (client.logged_in && client.file_descriptor >= 0) {
                receiver_online = true;
            }
            break;
        }
    }

    if (!receiver_known) {
        return;
    }

    if (is_blocked(sender_ip, reciever_ip)) {
        return;
    }

    if (receiver_online) {
        std::string payload_str =
            std::string("MESSAGE ") + sender_ip + ' ' + msg + '\n';
        const char* payload = payload_str.c_str();
        bool        sent    = send_payload_to_client(reciever_ip, payload);

        if (sent) {
            print_success("RELAYED");
            cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s",
                                  sender_ip,
                                  reciever_ip,
                                  msg);
            print_end("RELAYED");

            for (auto& client : clients) {
                if (client.ip == reciever_ip) {
                    client.messages_received++;
                }
                if (client.ip == sender_ip) {
                    client.messages_sent++;
                }
            }
        }
    } else {
        buffer_message(sender_ip, reciever_ip, msg);

        for (auto& client : clients) {
            if (client.ip == sender_ip) {
                client.messages_sent++;
            }
        }
    }
}

void send_broadcast(const char* sender_ip, const char* msg) {
    for (auto& client : clients) {
        if (client.ip == sender_ip) {
            continue;
        }

        bool blocked = is_blocked(sender_ip, client.ip.c_str());
        if (blocked) {
            continue;
        }

        if (client.logged_in && client.file_descriptor >= 0) {
            std::string payload_str =
                std::string("MESSAGE ") + sender_ip + ' ' + msg + '\n';
            const char* payload = payload_str.c_str();
            bool        sent    = send_payload_to_client(client.ip.c_str(),
                                                      payload);

            if (sent) {
                print_success("RELAYED");
                cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s",
                                      sender_ip,
                                      "255.255.255.255",
                                      msg);
                print_end("RELAYED");

                for (auto& c : clients) {
                    if (c.ip == client.ip) {
                        c.messages_received++;
                    }
                    if (c.ip == sender_ip) {
                        c.messages_sent++;
                    }
                }
            }
        } else {
            buffer_message(sender_ip, client.ip.c_str(), msg);

            for (auto& c : clients) {
                if (c.ip == sender_ip) {
                    c.messages_sent++;
                }
            }
        }
    }
}

void handle_block(const char* blocking_from, const char* ip_to_block) {
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, ip_to_block, &(sa.sin_addr)) != 1) {
        print_error("BLOCK");
        print_end("BLOCK");
        fprintf(stderr, "[BLOCK] Invalid IP address: %s\n", ip_to_block);
        return;
    }

    int is_client_in_vector = is_client_in_list_and_logged_in(ip_to_block);
    if (is_client_in_vector == 0) {
        print_error("BLOCK");
        print_end("BLOCK");
        fprintf(stderr,
                "[BLOCK] IP address: %s does not exist\n", ip_to_block);
        return;
    }

    for (auto& client : clients) {
        if (client.ip == blocking_from) {
            for (auto& blockedClients : client.blocked) {
                if (blockedClients == ip_to_block) {
                    print_error("BLOCK");
                    print_end("BLOCK");
                    fprintf(stderr,
                            "[BLOCK] IP address: %s already blocked\n",
                            ip_to_block);
                    return;
                }
            }

            client.blocked.push_back(ip_to_block);
            print_success("BLOCK");
            print_end("BLOCK");
            return;
        }
    }
}

void handle_blocked(const char* cmd) {
    char command[10], ip[20];
    sscanf(cmd, "%s %s", command, ip);

    struct sockaddr_in sa;
    if (inet_pton(AF_INET, ip, &(sa.sin_addr)) != 1) {
        print_error("BLOCKED");
        print_end("BLOCKED");
        fprintf(stderr, "[BLOCKED] Invalid IP address: %s\n", ip);
        return;
    }

    int is_client_in_vector = is_client_in_list_and_logged_in(ip);
    if (is_client_in_vector == 0) {
        print_error("BLOCKED");
        print_end("BLOCKED");
        fprintf(stderr, "[BLOCKED] IP address: %s does not exist\n", ip);
        return;
    }

    print_success("BLOCKED");

    int list_id = 1;

    for (auto& client : clients) {
        if (client.ip == ip) {
            for (const auto& c : client.blocked) {
                std::string      hostname = get_client_hostname(c.c_str());
                const char* blocked_ip = c.c_str();
                uint16_t    port       = get_client_peer_port(c.c_str());

                cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",
                                      list_id++,
                                      hostname.c_str(),
                                      blocked_ip,
                                      port);
            }
        }
    }

    print_end("BLOCKED");
}

void handle_unblock(const char* unblocking_from, const char* ip_to_unblock) {
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, ip_to_unblock, &(sa.sin_addr)) != 1) {
        print_error("UNBLOCK");
        print_end("UNBLOCK");
        fprintf(stderr, "[UNBLOCK] Invalid IP address: %s\n", ip_to_unblock);
        return;
    }

    int is_client_in_vector = is_client_in_list_and_logged_in(ip_to_unblock);
    if (is_client_in_vector == 0) {
        print_error("UNBLOCK");
        print_end("UNBLOCK");
        fprintf(stderr,
                "[UNBLOCK] IP address: %s does not exist\n", ip_to_unblock);
        return;
    }

    for (auto& client : clients) {
        if (client.ip == unblocking_from) {
            client.blocked.erase(
                std::remove(client.blocked.begin(),
                            client.blocked.end(),
                            ip_to_unblock),
                client.blocked.end());
        }
    }
}

static void handle_client_data(int current_fd, fd_set& master) {
    char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));

    ssize_t n = recv(current_fd, buf, sizeof(buf) - 1, 0);

    if (n <= 0) {
        FD_CLR(current_fd, &master);
        close(current_fd);

        for (auto& c : clients) {
            if (c.file_descriptor == current_fd) {
                c.file_descriptor = -1;
                c.logged_in       = false;
                break;
            }
        }
        return;
    }

    char     hostname[NI_MAXHOST], ip[INET_ADDRSTRLEN];
    uint16_t port;

    if (strncmp(buf, "EXIT", 4) == 0) {
        char command[10], exit_ip[INET_ADDRSTRLEN];
        if (sscanf(buf, "%s %s", command, exit_ip) == 2) {
            for (auto it = clients.begin(); it != clients.end();) {
                if (it->ip == exit_ip) {
                    it = clients.erase(it);
                } else {
                    ++it;
                }
            }

            for (auto it = buffered_messages.begin();
                 it != buffered_messages.end();) {
                if (it->sender_ip == exit_ip || it->receiver_ip == exit_ip) {
                    it = buffered_messages.erase(it);
                } else {
                    ++it;
                }
            }
        }
        FD_CLR(current_fd, &master);
        close(current_fd);
        return;
    }

    if (sscanf(buf, "HELLO %s %s %" SCNu16, hostname, ip, &port) == 3) {
        ClientInfo* existing = nullptr;
        for (auto& cl : clients) {
            if (cl.ip == ip) {
                existing = &cl;
                break;
            }
        }

        if (existing) {
            existing->file_descriptor = current_fd;
            existing->hostname        = hostname;
            existing->peer_port       = port;
            existing->logged_in       = true;
        } else {
            ClientInfo c;
            c.file_descriptor = current_fd;
            c.hostname        = hostname;
            c.ip              = ip;
            c.peer_port       = port;
            c.logged_in       = true;
            clients.push_back(c);
        }

        send_client_list(current_fd);

        for (auto it = buffered_messages.begin();
             it != buffered_messages.end();) {
            if (it->receiver_ip == std::string(ip)) {
                std::string payload_str =
                    std::string("MESSAGE ") + it->sender_ip + ' ' +
                    it->msg + '\n';
                const char* payload = payload_str.c_str();
                bool        sent    = send_payload_to_client(ip, payload);

                if (sent) {
                    print_success("RELAYED");
                    cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s",
                                          it->sender_ip.c_str(),
                                          it->receiver_ip.c_str(),
                                          it->msg.c_str());
                    print_end("RELAYED");

                    for (auto& cl : clients) {
                        if (cl.ip == it->receiver_ip) {
                            cl.messages_received++;
                        }
                    }
                }

                it = buffered_messages.erase(it);
            } else {
                ++it;
            }
        }

        return;
    } else if (strncmp(buf, "REFRESH", 7) == 0) {
        send_client_list(current_fd);
    } else if (strncmp(buf, "BLOCK", 5) == 0) {
        char command[10], blocking_from[20], ip_to_block[20];
        sscanf(buf, "%s %s %s", command, blocking_from, ip_to_block);
        handle_block(blocking_from, ip_to_block);
    } else if (strncmp(buf, "UNBLOCK", 7) == 0) {
        char command[10], unblocking_from[20], ip_to_unblock[20];
        sscanf(buf, "%s %s %s", command, unblocking_from, ip_to_unblock);
        handle_unblock(unblocking_from, ip_to_unblock);
    } else if (strncmp(buf, "MESSAGE", 7) == 0) {
        char trash[10], sender_ip[20], reciever_ip[20];
        char msg[256];

        sscanf(buf, "%s %s %s", trash, sender_ip, reciever_ip);

        int msg_index = 0;
        for (size_t i = strlen(trash) + strlen(sender_ip) +
                         strlen(reciever_ip) + 3;
             buf[i] != '\0';
             i++) {
            msg[msg_index++] = buf[i];
        }
        msg[msg_index] = '\0';

        send_message(sender_ip, reciever_ip, msg);
    } else if (strncmp(buf, "BROADCAST", 9) == 0) {
        char trash[10], sender_ip[20];
        char msg[256];

        sscanf(buf, "%s %s", trash, sender_ip);

        int msg_index = 0;
        for (size_t i = strlen(trash) + strlen(sender_ip) + 2;
             buf[i] != '\0';
             i++) {
            msg[msg_index++] = buf[i];
        }
        msg[msg_index] = '\0';

        send_broadcast(sender_ip, msg);
    }
}

int run_server(const char* port_str) {
    my_ip = get_ip_address();
    listen_file_descriptor_number = make_listen_socket(port_str);

    if (listen_file_descriptor_number < 0) {
        return 1;
    }

    fd_set current;
    fd_set readfds;
    FD_ZERO(&current);
    FD_ZERO(&readfds);

    FD_SET(listen_file_descriptor_number, &current);
    FD_SET(STDIN_FD, &current);

    int max_fd = listen_file_descriptor_number;

    while (true) {
        readfds = current;

        int rv = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (rv == -1) {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FD, &readfds)) {
            char cmd[CMD_SIZE];
            memset(cmd, 0, CMD_SIZE);

            if (fgets(cmd, CMD_SIZE - 1, stdin) == NULL) {
                break;
            }

            cmd[strcspn(cmd, "\n")] = 0;

            char command[CMD_SIZE];
            sscanf(cmd, "%s", command);

            if (strcmp(command, "AUTHOR") == 0) {
                handle_author();
            } else if (strcmp(command, "IP") == 0) {
                handle_ip();
            } else if (strcmp(command, "PORT") == 0) {
                handle_port();
            } else if (strcmp(command, "LIST") == 0) {
                handle_list_server();
            } else if (strcmp(command, "STATISTICS") == 0) {
                handle_statistics_server();
            } else if (strcmp(command, "BLOCKED") == 0) {
                handle_blocked(cmd);
            }
        }

        for (int i = 0; i <= max_fd; i++) {
            if (i == STDIN_FD || i == listen_file_descriptor_number)
                continue;

            if (FD_ISSET(i, &readfds)) {
                handle_client_data(i, current);
            }
        }

        if (FD_ISSET(listen_file_descriptor_number, &readfds)) {
            accept_new_client(current, max_fd);
        }
    }

    for (auto& c : clients)
        close(c.file_descriptor);
    if (listen_file_descriptor_number >= 0) {
        close(listen_file_descriptor_number);
    }

    return 0;
}
