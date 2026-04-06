/**
* @author  Team Members jcibarra dabrober
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
#include <unordered_map>

#include "../include/global.h"
#include "../include/logger.h"
#include "../include/client.h"

#define STDIN_FD 0
#define CMD_SIZE 100
#define MAX_MSG_SIZE 256

using namespace std;

extern void print_success(const char* cmd);
extern void print_error(const char* cmd);
extern void print_end(const char* cmd);
extern void handle_author();
extern void handle_ip();
extern void handle_port();
extern string get_ip_address();

extern vector<ClientInfo> clients;
extern uint16_t listen_port_number;
extern string my_ip;

static int server_socket_fd = -1;
static bool logged_in = false;

extern int is_client_in_list_and_logged_in(const std::string& ip);

bool send_payload_to_server(const char* payload) {
    ssize_t sent = send(server_socket_fd, payload, strlen(payload), 0);
    if (sent <= 0) return false;
    return true;
}

void handle_list_client() {
    if (!logged_in) {
        print_error("LIST");
        print_end("LIST");
        return;
    }
    
    print_success("LIST");
    
    vector<ClientInfo> sorted = clients;
    sort(sorted.begin(), sorted.end(), 
         [](const ClientInfo& a, const ClientInfo& b) {
             return a.peer_port < b.peer_port;
         });
    
    int list_id = 1;
    for (const auto& c : sorted) {
        const char* hostname = (!c.hostname.empty()) ? c.hostname.c_str()
                                                     : c.ip.c_str();
        
        cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",
                              list_id++,
                              hostname,
                              c.ip.c_str(),
                              c.peer_port);
    }
    
    print_end("LIST");
}

void handle_exit() {
    if (logged_in && server_socket_fd >= 0) {
        std::string payload_str = std::string("EXIT ") + my_ip + "\n";
        send(server_socket_fd, payload_str.c_str(), payload_str.size(), 0);

        close(server_socket_fd);
        server_socket_fd = -1;
        logged_in = false;
    }

    print_success("EXIT");
    print_end("EXIT");
}

void handle_login(char* cmd, fd_set& master_fds, ClientInfo client) {
    char command[10], server_ip[20], server_port[16] = {0};
    sscanf(cmd, "%s %s %s", command, server_ip, server_port);

    int port_num = 0;
    try {
        port_num = stoi(server_port);
    } catch (...) {
        print_error("LOGIN");
        print_end("LOGIN");
        fprintf(stderr, "[LOGIN] Invalid port number: %s\n", server_port);
        return;
    }

    if (port_num < 1 || port_num > 65535) {
        print_error("LOGIN");
        print_end("LOGIN");
        fprintf(stderr, "[LOGIN] Port out of range: %d\n", port_num);
        return;
    }

    struct sockaddr_in sa;
    if (inet_pton(AF_INET, server_ip, &(sa.sin_addr)) != 1) {
        print_error("LOGIN");
        print_end("LOGIN");
        fprintf(stderr, "[LOGIN] Invalid IP address: %s\n", server_ip);
        return;
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(server_ip, server_port, &hints, &res);
    if (gai != 0) {
        fprintf(stderr, "[LOGIN] getaddrinfo: %s\n", gai_strerror(gai));
        return;
    }

    int sockfd = -1;
    for (auto p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            server_socket_fd = sockfd;
            FD_SET(sockfd, &master_fds);
            
            print_success("LOGIN");

            char hello_msg[100];
            snprintf(hello_msg, sizeof(hello_msg),
                     "HELLO %s %s %d\n", 
                     client.hostname.c_str(), my_ip.c_str(), client.peer_port);
            
            ssize_t sent = send(sockfd, hello_msg, strlen(hello_msg), 0);
            (void)sent;

            logged_in = true;
            break;
        }
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd < 0) {
        fprintf(stderr, "[LOGIN] Failed to connect\n");
        print_error("LOGIN");
    }
    
    client.logged_in = true;
    print_end("LOGIN");
}

void handle_refresh() {
    if (!logged_in || server_socket_fd < 0) {
        print_error("REFRESH");
        print_end("REFRESH");
        return;
    }
    
    const char* refresh_msg = "REFRESH\n";
    ssize_t sent = send(server_socket_fd, refresh_msg, strlen(refresh_msg), 0);
    
    if (sent <= 0) {
        print_error("REFRESH");
        print_end("REFRESH");
        return;
    }
    
    print_success("REFRESH");
    print_end("REFRESH");
}

bool handle_server_message() {
    char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));

    ssize_t n = recv(server_socket_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        return false;
    }

    bool cleared_list = false;

    char* saveptr = nullptr;
    char* line    = strtok_r(buf, (char*)"\n", &saveptr);

    while (line != nullptr) {
        if (strncmp(line, "MESSAGE", 7) == 0) {
            char sender_ip[INET_ADDRSTRLEN];
            sscanf(line, "MESSAGE %s", sender_ip);

            const char* msg_start = line;
            msg_start = strchr(msg_start, ' ');
            if (msg_start) msg_start++;
            msg_start = strchr(msg_start, ' ');
            if (msg_start) msg_start++;

            if (msg_start) {
                print_success("RECEIVED");
                cse4589_print_and_log("msg from:%s\n[msg]:%s\n",
                                      sender_ip, msg_start);
                print_end("RECEIVED");
            }
        } else {
            ClientInfo c;
            char hostname[NI_MAXHOST], ip[INET_ADDRSTRLEN];
            uint16_t port;

            if (sscanf(line, "%s %s %" SCNu16, hostname, ip, &port) == 3) {
                if (!cleared_list) {
                    clients.clear();
                    cleared_list = true;
                }
                c.hostname        = hostname;
                c.ip              = ip;
                c.peer_port       = port;
                c.file_descriptor = -1;
                c.logged_in       = true;
                clients.push_back(c);
            }
        }

        line = strtok_r(nullptr, (char*)"\n", &saveptr);
    }

    return true;
}

void handle_block(char* cmd, int server_fd) {
    char command[10], ip_to_block[20];
    sscanf(cmd, "%s %s", command, ip_to_block);

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
        fprintf(stderr, "[BLOCK] IP address: %s does not exist\n", ip_to_block);
        return;
    }

    for (auto& client : clients) {
        if (client.ip == my_ip) {
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
        }
    }

    char block_msg[100];
    snprintf(block_msg, sizeof(block_msg),
             "BLOCK %s %s\n", my_ip.c_str(), ip_to_block);
    bool sent = send_payload_to_server(block_msg);
    
    if (sent) print_success("BLOCK");
    else      print_error("BLOCK");
    print_end("BLOCK");
}

void handle_unblock(char* cmd, int server_fd) {
    char command[10], ip_to_unblock[20];
    sscanf(cmd, "%s %s", command, ip_to_unblock);

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
        if (client.ip == my_ip) {
            client.blocked.erase(
                std::remove(client.blocked.begin(),
                            client.blocked.end(),
                            ip_to_unblock),
                client.blocked.end());
        }
    }

    char unblock_msg[100];
    snprintf(unblock_msg, sizeof(unblock_msg),
             "UNBLOCK %s %s\n", my_ip.c_str(), ip_to_unblock);
    bool sent = send_payload_to_server(unblock_msg);
    
    if (sent) print_success("UNBLOCK");
    else      print_error("UNBLOCK");
    print_end("UNBLOCK");
}

void handle_send(char* cmd, fd_set& master_fds, ClientInfo client) {
    char command[10], reciever_ip[20];
    char msg[MAX_MSG_SIZE];
    memset(msg, 0, sizeof(msg));

    if (sscanf(cmd, "%s %s", command, reciever_ip) != 2) {
        print_error("SEND");
        print_end("SEND");
        fprintf(stderr, "[SEND] Usage: SEND <client-ip> <msg>\n");
        return;
    }

    size_t offset  = strlen(command) + strlen(reciever_ip) + 2;
    size_t cmd_len = strlen(cmd);
    if (offset >= cmd_len) {
        print_error("SEND");
        print_end("SEND");
        fprintf(stderr, "[SEND] Empty message\n");
        return;
    }

    size_t msg_index = 0;
    for (size_t i = offset; i < cmd_len && msg_index < MAX_MSG_SIZE - 1; i++) {
        msg[msg_index++] = cmd[i];
    }
    msg[msg_index] = '\0';

    if (strlen(msg) == 0) {
        print_error("SEND");
        print_end("SEND");
        fprintf(stderr, "[SEND] Empty message\n");
        return;
    }
    if (strlen(msg) >= MAX_MSG_SIZE) {
        print_error("SEND");
        print_end("SEND");
        fprintf(stderr, "[SEND] Message too long: %zu (max %d)\n",
                strlen(msg), MAX_MSG_SIZE - 1);
        return;
    }

    int checker = is_client_in_list_and_logged_in(reciever_ip);
    if (checker == 0) {
        print_error("SEND");
        print_end("SEND");
        fprintf(stderr,
                "[SEND] IP address does not exist in list\n");
        return;
    }

    struct sockaddr_in sa;
    if (inet_pton(AF_INET, reciever_ip, &(sa.sin_addr)) != 1) {
        print_error("SEND");
        print_end("SEND");
        fprintf(stderr, "[SEND] Invalid IP address: %s\n", reciever_ip);
        return;
    }

    std::string payload_str = std::string("MESSAGE ") + my_ip + ' ' +
                              reciever_ip + ' ' + msg + '\n';
    const char* payload = payload_str.c_str();

    ssize_t sent = send(server_socket_fd, payload, strlen(payload), 0);
    if (sent <= 0) {
        print_error("SEND");
        print_end("SEND");
        fprintf(stderr, "[SEND] send() to server failed\n");
        return;
    }

    print_success("SEND");
    print_end("SEND");
}

void handle_broadcast(char* cmd, fd_set& master_fds, ClientInfo client) {
   char command[10];
   char msg[256];
   sscanf(cmd, "%s", command);

   int msg_index = 0;
   for (size_t i = strlen(command) + 1; cmd[i] != '\0'; i++) {
       msg[msg_index++] = cmd[i];
   }
   msg[msg_index] = '\0';

   if (strlen(msg) > 256) {
       print_error("BROADCAST");
       print_end("BROADCAST");
       fprintf(stderr, "Message too long: %zu (max 256)\n", strlen(msg));
       return;
   }

   if (strlen(msg) == 0) {
       print_error("BROADCAST");
       print_end("BROADCAST");
       fprintf(stderr, "Empty message\n");
       return;
   }

   string payload_str = string("BROADCAST ") + my_ip + ' ' + msg + '\n';
   const char* payload = payload_str.c_str();

   bool sent = send_payload_to_server(payload);

   if (!sent) {
       print_error("BROADCAST");
       print_end("BROADCAST");
       return;
   }

   print_success("BROADCAST");
   print_end("BROADCAST");
}

int run_client(const char* port_str) {
    listen_port_number = static_cast<uint16_t>(stoi(port_str));
    my_ip = get_ip_address();

    char hostname[NI_MAXHOST];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        strncpy(hostname, "unknown", sizeof(hostname));
    }

    ClientInfo client;
    client.file_descriptor = -1;
    client.ip        = my_ip;
    client.peer_port = static_cast<uint16_t>(stoi(string(port_str)));
    client.hostname  = hostname;
    client.logged_in = false;

    fd_set master_fds;
    fd_set read_fds;
    
    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FD, &master_fds);

    char sendSlice[5];
    
    while (true) {
        read_fds = master_fds;
        
        int max_fd = STDIN_FD;
        if (server_socket_fd >= 0 && server_socket_fd > max_fd) {
            max_fd = server_socket_fd;
        }
        
        int rv = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (rv < 0) {
            perror("select");
            break;
        }
        
        if (FD_ISSET(STDIN_FD, &read_fds)) {
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
            }
            else if (strcmp(command, "IP") == 0) {
                handle_ip();
            }
            else if (strcmp(command, "PORT") == 0) {
                handle_port();
            }
            else if (strcmp(command, "EXIT") == 0) {
                handle_exit();
                return 0;
            }
            else if (strcmp(command, "LOGIN") == 0) {
                handle_login(cmd, master_fds, client);
            }
            else if (strcmp(command, "LOGOUT") == 0) {
                if (!logged_in || server_socket_fd < 0) {
                    print_error("LOGOUT");
                    print_end("LOGOUT");
                } else {
                    int old_fd = server_socket_fd;
                    close(server_socket_fd);
                    FD_CLR(old_fd, &master_fds);
                    server_socket_fd = -1;
                    logged_in = false;

                    print_success("LOGOUT");
                    print_end("LOGOUT");
                }
            }
            else if (strcmp(command, "LIST") == 0) {
                if (!logged_in) {
                    print_error("LIST");
                    print_end("LIST");
                } else {
                    handle_list_client();
                }
            }
            else if (strcmp(command, "REFRESH") == 0) {
                if (!logged_in) {
                    print_error("REFRESH");
                    print_end("REFRESH");
                } else {
                    handle_refresh();
                }
            }
            else if (strcmp(command, "BLOCK") == 0) {
                if (!logged_in) {
                    print_error("BLOCK");
                    print_end("BLOCK");
                } else {
                    handle_block(cmd, server_socket_fd);
                }
            }
            else if (strcmp(command, "UNBLOCK") == 0) {
                if (!logged_in) {
                    print_error("UNBLOCK");
                    print_end("UNBLOCK");
                } else {
                    handle_unblock(cmd, server_socket_fd);
                }
            }
            else if (strcmp(command, "BROADCAST") == 0) {
                if (!logged_in) {
                    print_error("BROADCAST");
                    print_end("BROADCAST");
                } else {
                    handle_broadcast(cmd, master_fds, client);
                }
            }
            else {
                for (int i = 0; i < 4; i++)
                    sendSlice[i] = cmd[i];
                sendSlice[4] = '\0';

                if (strcmp(sendSlice, "SEND") == 0) {
                    if (!logged_in) {
                        print_error("SEND");
                        print_end("SEND");
                    } else {
                        handle_send(cmd, master_fds, client);
                    }
                } else {
                    fprintf(stderr, "Unknown command: %s\n", command);
                }
            }
        }
        
        if (server_socket_fd >= 0 && FD_ISSET(server_socket_fd, &read_fds)) {
            if (!handle_server_message()) {
                int old_fd = server_socket_fd;
                close(server_socket_fd);
                server_socket_fd = -1;
                FD_CLR(old_fd, &master_fds);
                logged_in = false;
                printf("Server connection lost\n");
            }
        }
    }
    
    if (server_socket_fd >= 0) {
        close(server_socket_fd);
    }
    return 0;
}
