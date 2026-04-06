#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <sys/select.h>
#include <stdint.h>

struct ClientInfo {
    int file_descriptor;
    std::string ip;
    std::string hostname;
    uint16_t peer_port;
    int messages_sent = 0;
    int messages_received = 0;
    bool logged_in = true;
    std::vector<std::string> blocked;
};

struct BufferedMessage {
    std::string sender_ip;
    std::string receiver_ip;
    std::string msg;
};

extern std::vector<BufferedMessage> buffered_messages;

int run_server(const char* port_str);
void handle_list_server();
void handle_statistics_server();
void handle_blocked(const char* cmd);
void send_message(const char* sender_ip, const char* reciever_ip, const char* msg);
void send_broadcast(const char* sender_ip, const char* msg);
void handle_block(const char* blocking_from, const char* ip_to_block);
void handle_unblock(const char* unblocking_from, const char* ip_to_unblock);
bool send_payload_to_client(const char* reciever_ip, const char* payload);


#endif
