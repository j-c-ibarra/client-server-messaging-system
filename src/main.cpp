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
*/
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../include/global.h"
#include "../include/logger.h"
#include "../include/server.h"
#include "../include/client.h"

using namespace std;

vector<ClientInfo> clients;
vector<BufferedMessage> buffered_messages;
uint16_t listen_port_number = 0;
string my_ip;

int is_client_in_list_and_logged_in(const std::string& ip) {
    for (const auto& client : clients) {
        if(client.ip == ip) {
            if(client.logged_in){
                return 2;
            } else {
                return 1;
            }
        }
    }
    return 0;
}

void print_success(const char* cmd) {
    cse4589_print_and_log("[%s:SUCCESS]\n", cmd);
}

void print_error(const char* cmd) {
    cse4589_print_and_log("[%s:ERROR]\n", cmd);
}

void print_end(const char* cmd) {
    cse4589_print_and_log("[%s:END]\n", cmd);
}

void handle_port() {
    print_success("PORT");
    cse4589_print_and_log("PORT:%d\n", listen_port_number);
    print_end("PORT");
}

string get_ip_address() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return "";
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "";
    }
    
    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    getsockname(sock, (struct sockaddr*)&local_addr, &len);
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, sizeof(ip_str));
    
    close(sock);
    return string(ip_str);
}

void handle_ip() {
    if (my_ip.empty()) {
        my_ip = get_ip_address();
    }
    
    if (my_ip.empty()) {
        print_error("IP");
        print_end("IP");
        return;
    }
    
    print_success("IP");
    cse4589_print_and_log("IP:%s\n", my_ip.c_str());
    print_end("IP");
}

void handle_author() {
    print_success("AUTHOR");
    cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n", 
                          "jcibarra-dabrober");
    print_end("AUTHOR");
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s s <port>\n", argv[0]);
        return 1;
    }
    
    cse4589_init_log(argv[2]);
    fclose(fopen(LOGFILE, "w"));

    if ((static_cast<string>(argv[1])) == "s") {
        return run_server(argv[2]);
    } else if ((static_cast<string>(argv[1])) == "c") {
        return run_client(argv[2]);
    } else {
        fprintf(stderr, "Must use %s s <port>\n", argv[0]);
        return 1;
    }
}
