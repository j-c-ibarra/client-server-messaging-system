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
#ifndef CLIENT_H
#define CLIENT_H

#include <sys/select.h>
#include "server.h"

int run_client(const char* port_str);
void handle_login(char* cmd, fd_set& master_fds, ClientInfo client);
void handle_refresh();
void handle_list_client();
bool handle_server_message();
void handle_exit();
void handle_send(char* cmd, fd_set& master_fds, ClientInfo client);
void handle_broadcast(char* cmd, fd_set& master_fds, ClientInfo client);
void handle_block(char* cmd, int server_fd);
void handle_unblock(char* cmd, int server_fd);
int is_client_in_list_and_logged_in(const std::string& ip);


#endif
