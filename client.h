#ifndef _CLIENT_H
#define _CLIENT_H

#include <sys/types.h>

using namespace std;

typedef enum CLIENT_STATE_tag
{
    WAITING = 0,
    PROCESS_LS = 1,
    PROCESS_SEND = 2,
    PROCESS_GET = 3,
    PROCESS_REMOVE = 4,
    PROCESS_RENAME = 5,
    SHUTDOWN = 6,
    QUIT = 7
}Client_State_T;

// Function prototypes
int process_ls(int sockfd, const struct sockaddr_in& server_addr, uint16_t udp_port);

int process_send(int sockfd, struct sockaddr_in& server_addr, const std::string& in_cmd, int udp_port);

int process_remove(int sockfd, struct sockaddr_in& server_addr, const std::string& in_cmd);

int process_rename(int sockfd, struct sockaddr_in& server_addr, char filenames[FILE_NAME_LEN]);

int process_shutdown(int sockfd, struct sockaddr_in& server_addr);

#endif
