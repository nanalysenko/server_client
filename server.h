#ifndef _SERVER_H
#define _SERVER_H

#include <vector>
#include <string>

using namespace std;

typedef enum SERVER_STATE_tag
{
    WAITING = 0,
    PROCESS_LS = 1,
    PROCESS_SEND = 2,
    PROCESS_GET = 3,
    PROCESS_REMOVE = 4,
    PROCESS_RENAME = 5,
    SHUTDOWN = 6
}Server_State_T;

bool checkFile(const char *fileName);
int checkDirectory (string dir);
int getDirectory (string dir, vector<string> &files);

int process_ls(int sockfd, struct sockaddr_in& client_addr);

int process_send(int udp_sockfd, struct sockaddr_in& client_addr, const Cmd_Msg_T& msg);

int process_remove(int udp_sockfd, struct sockaddr_in& client_addr, const Cmd_Msg_T& msg);

int process_shutdown(int udp_sockfd, struct sockaddr_in& client_addr);

int process_rename(int udp_sockfd, struct sockaddr_in& client_addr, const char filename[PATH_MAX], const char new_filename[PATH_MAX]);

#endif
