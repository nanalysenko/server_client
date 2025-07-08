#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <string.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <string>
#include <random>



#include "message.h"
#include "server.h"

using namespace std;

Server_State_T server_state;
string cmd_string[] = {" ", "CMD_LS", "CMD_SEND","CMD_GET","CMD_REMOVE","CMD_RENAME","CMD_SHUTDOWN"};

std::string getCurrentDirectoryWithSubdir(const std::string &subdir) {
    char buffer[PATH_MAX];

    // Get the current working directory
    if (getcwd(buffer, sizeof(buffer)) != NULL) {
        // Ensure the path ends with a slash
        if (buffer[strlen(buffer) - 1] != '/') {
            strcat(buffer, "/"); // Add a slash if not present
        }
        strcat(buffer, subdir.c_str()); // Concatenate the subdirectory
        return std::string(buffer); // Return the full path as a string
    } else {
        perror("getcwd() error");
        return ""; // Return an empty string on error
    }
}

int process_ls(int sockfd, struct sockaddr_in& client_addr) {
    Cmd_Msg_T msg;
    socklen_t client_addr_len = sizeof(client_addr);
    // get path to current directory
    vector<string> files;
    std::string directory_path = getCurrentDirectoryWithSubdir("backup/");
    int check_dir_res = checkDirectory(directory_path);
    if (check_dir_res!=0){
        // get files in the directory
        cerr << "Error getting directory: " << strerror(check_dir_res) << endl;
        return -1;
    }          
    int check_files_res = getDirectory(directory_path, files);
    if (check_files_res!= 0) {
        cerr << "Error getting directory: " << strerror(check_files_res) << endl;
        return -1;
    }
    msg.cmd = 1;
    msg.size = htonl(files.size());
    // send back number of files in the backup folder
    ssize_t sent_bytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, client_addr_len);
    if (sent_bytes < 0) {
        std::cerr << " - send failed: " << strerror(errno) << std::endl;
        return -1; // Indicate failure
    }
    // check if there are any files in the folder 
    if (files.size()==0) {
        std::cout << " - server backup folder is empty." << endl;
        return 0;
    } 
    // send file names
    for (const auto &file : files) {
        strncpy(msg.filename, file.c_str(), FILE_NAME_LEN-1);
        msg.filename[FILE_NAME_LEN - 1] = '\0';
        msg.cmd = 1;
        msg.size = htonl(files.size());
        ssize_t sent_bytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, client_addr_len);
        if (sent_bytes < 0) {
            std::cerr << " - send failed: " << strerror(errno) << std::endl;
            return -1; // Indicate failure
        }
        std::cout << " - " << msg.filename << endl;
    }
    return 0;
}

int process_send(int udp_sockfd, struct sockaddr_in& client_addr, const Cmd_Msg_T& msg) {
    Cmd_Msg_T sendMsg;
    char filename[PATH_MAX];
    char filepath[PATH_MAX];
    socklen_t client_addr_len = sizeof(client_addr); 
    // accept file name msg
    strncpy(filename, msg.filename, PATH_MAX-1);
    filename[PATH_MAX - 1] = '\0';
    // check if folder exists empty/not empty
    std::string directory_path = getCurrentDirectoryWithSubdir("backup/");
    int check_dir_res = checkDirectory(directory_path);
    if(check_dir_res!=0){
        return -1; 
    }
    strcpy(filepath, directory_path.c_str());
    strcat(filepath, filename); // Concatenate the subdirectory
    // std::cout << filepath << std::endl;
    int check_file_res = checkFile(filepath);
    // std::cout << check_file_res << std::endl;
    // file exists 
    if(check_file_res==1){
        sendMsg.cmd = 2;
        sendMsg.error = htons(2); //512
        ssize_t sent_bytes_1 = sendto(udp_sockfd, &sendMsg, sizeof(sendMsg), 0, (struct sockaddr*)&client_addr, client_addr_len);
        if (sent_bytes_1 < 0) {
            std::cerr << "- send failed: " << strerror(errno) << std::endl;
            return -1; // Indicate failure
        }
        std::cerr << " - file ./backup/" << filename << " exist; overwrite?" << std::endl;
        recvfrom(udp_sockfd, &sendMsg, sizeof(sendMsg), 0, (struct sockaddr*)&client_addr, &client_addr_len);
        if(htons(sendMsg.error)==2) {
            return 0; 
        } 
        std::cerr << "- overwrite. " << std::endl;
        // Attempt to delete the file
        if (remove(filepath) != 0) {
            perror("Error deleting file");
            return -1;  // Indicate failure
        }
        check_file_res = 0;
    } if (check_file_res==0) {
        // Create TCP socket
        int tcp_port;
        struct sockaddr_in tcp_addr;
        int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        // create socket
        if (tcp_sockfd < 0) {
            perror("TCP socket creation failed");
            close(tcp_sockfd);
            return -1; // Indicate failure
        }
        memset(&tcp_addr, 0, sizeof(tcp_addr));
        tcp_addr.sin_family = AF_INET;
        tcp_addr.sin_addr.s_addr = INADDR_ANY;
        tcp_addr.sin_port = htons(0);
        // Bind TCP socket
        if (bind(tcp_sockfd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
            perror("TCP bind failed");
            close(tcp_sockfd);
            return -1; // Indicate failure
        }
        // Get the assigned port number
        socklen_t addr_len = sizeof(tcp_addr);
        if (getsockname(tcp_sockfd, (struct sockaddr *)&tcp_addr, &addr_len) == -1) {
            perror("getsockname failed");
            close(tcp_sockfd);
            return -1; // Indicate failure
        }
        // Listen on TCP socket
        if (listen(tcp_sockfd, 1) < 0) {
            perror("TCP listen failed");
            close(tcp_sockfd);
            return -1; // Indicate failure
        }
        // retreive port number 
        tcp_port = ntohs(tcp_addr.sin_port) ;
        std::cout <<  " - listen @: " << tcp_port <<std::endl;
        // send port number 
        sendMsg.cmd = 2;
        sendMsg.port = htons(tcp_port);
        sendMsg.error = 0;
        ssize_t sent_bytes_0 = sendto(udp_sockfd, &sendMsg, sizeof(sendMsg), 0, (struct sockaddr*)&client_addr, client_addr_len);
        if (sent_bytes_0 < 0) {
            std::cerr << " - send failed: " << strerror(errno) << std::endl;
            return -1; // Indicate failure
        }
        // accept handshaking
        int client_sockfd = accept(tcp_sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sockfd < 0) {
            perror("Accept failed");
            close(tcp_sockfd);
            return -1; // Indicate failure
        }
        std::cout << " - connected with the client." << std::endl;
        // accept file transfer
        // Open file to write received data
        std::ofstream outfile(filepath, std::ios::binary);
        // Resize the file to the specified filesize
        outfile.seekp(ntohl(msg.size) - 1); // Move to the last byte
        outfile.write("", 1); // Write a single byte to create the file of specified size
        if (!outfile.is_open()) {
            std::cerr << "- open file "<< msg.filename << " error." << std::endl;
            close(tcp_sockfd);
            return -1; // Indicate failure
        }
        std::cout << " - filename: "<< msg.filename  << std::endl;
        std::cout << " - filesize "<< htonl(msg.size) << std::endl;
        char buffer[DATA_BUF_LEN];
        ssize_t bytes_received;
        ssize_t total_bytes_received = 0;
        // Receive data in chunks
        int error = 0;
        while (total_bytes_received < htonl(msg.size)) {
            ssize_t bytes_received = recv(client_sockfd, buffer, DATA_BUF_LEN, 0);
            if (bytes_received <= 0) {
                error = 1;
                std::cerr << "- message reception error." << std::endl; // Error handling
                } // Connection closed or error
            std::cout << bytes_received << std::endl;
            outfile.write(buffer, bytes_received);
            total_bytes_received += bytes_received;
            std::cout << " - total bytes received: " << total_bytes_received << std::endl;
        }
        outfile.close();
        std::cout << " - " << msg.filename << " has been received." << std::endl;
        sendMsg.cmd = CMD_ACK;
        sendMsg.error = htons(error);
        sendto(udp_sockfd, &sendMsg, sizeof(sendMsg), 0, (struct sockaddr*)&client_addr, client_addr_len);
        std::cout << " - send acknowledgement." << std::endl;
        close(client_sockfd);
        close(tcp_sockfd);

    }
    return 0;
}

int process_remove(int udp_sockfd, struct sockaddr_in& client_addr, const Cmd_Msg_T& msg) {
    Cmd_Msg_T removeMsg;
    char filename[PATH_MAX];
    char filepath[PATH_MAX];
    socklen_t client_addr_len = sizeof(client_addr); 
    // accept file name msg
    strncpy(filename, msg.filename, PATH_MAX-1);
    filename[PATH_MAX - 1] = '\0';
    std::string directory_path = getCurrentDirectoryWithSubdir("backup/");
    int check_dir_res = checkDirectory(directory_path);
    if(check_dir_res!=0){
        return -1; 
    }
    strcpy(filepath, directory_path.c_str());
    strcat(filepath, filename); // Concatenate the subdirectory
    // std::cout << filepath << std::endl;
    int check_file_res = checkFile(filepath);
    if (check_file_res==0) {
        std::cout << " - file does not exist." << std::endl;
        removeMsg.cmd = CMD_REMOVE;
        removeMsg.error = 1;
        ssize_t sent_bytes = sendto(udp_sockfd, &removeMsg, sizeof(removeMsg),  0, (struct sockaddr*)&client_addr, client_addr_len);
        if (sent_bytes < 0) {
            std::cerr << " - send failed: " << strerror(errno) << std::endl;
            return -1; // Indicate failure
        }
        return 0;
    } 
    if (remove(filepath) != 0) {
        perror("Error deleting file");
        return -1;  // Indicate failure
    }
    std::cerr << " - removed: " << msg.filename << std::endl;
    removeMsg.cmd = CMD_REMOVE;
    removeMsg.error = 0;
    ssize_t sent_bytes = sendto(udp_sockfd, &removeMsg, sizeof(removeMsg),  0, (struct sockaddr*)&client_addr, client_addr_len);
    if (sent_bytes < 0) {
        std::cerr << " - send failed: " << strerror(errno) << std::endl;
        return -1; // Indicate failure
    }
    std::cout << " - send acknowledgemet." << std::endl;
    return 0;
}

int process_shutdown(int udp_sockfd, struct sockaddr_in& client_addr) {
    Cmd_Msg_T shutdownMsg;
    shutdownMsg.cmd =CMD_ACK;
    shutdownMsg.error = htons(0);
    socklen_t client_addr_len = sizeof(client_addr); 
    ssize_t sent_bytes = sendto(udp_sockfd, &shutdownMsg, sizeof(shutdownMsg), 0, (struct sockaddr*)&client_addr, client_addr_len);
    if (sent_bytes < 0) {
        std::cerr << " - send failed: " << strerror(errno) << std::endl;
        return -1;// Indicate failure
    }
    std::cout << " - send acknowledgemet." << std::endl;
    close(udp_sockfd);
    return 0;
}

int process_rename(int udp_sockfd, struct sockaddr_in& client_addr, const char filename[PATH_MAX], const char new_filename[PATH_MAX]) {
    Cmd_Msg_T renameMsg;
    socklen_t client_addr_len = sizeof(client_addr);
    int error = 0;
    int check_file_res = checkFile(filename);
    if(check_file_res==0){
        std::cout << " - file doesn’t exist." << std::endl;
        error = 1;
    } else {
        // Rename the file
        if (rename(filename, new_filename) != 0) {
            perror("Error renaming file");  // Print error message
            return -1; 
        } 
        std::cout << " - the file has been renamed to "<< new_filename << std::endl;
    }
    renameMsg.cmd = 8;
    renameMsg.error = htons(error);
    ssize_t sent_bytes = sendto(udp_sockfd, &renameMsg, sizeof(renameMsg), 0, (struct sockaddr*)&client_addr, client_addr_len);
    if (sent_bytes < 0) {
        std::cerr << " - send failed: " << strerror(errno) << std::endl;
        return -1;
    }
    std::cout << " - send acknowledgemet." << std::endl;
    return 0;   
}

int main(int argc, char *argv[])
{
    unsigned short udp_port = 0;
	if ((argc != 1) && (argc != 3))
	{
		cout << "Usage: " << argv[0];
		cout << " [-port <udp_port>]" << endl;
		return 1;
	}
	else
	{   //system("clear");
		//process input arguments
		for (int i = 1; i < argc; i++)
		{				
			if (strcmp(argv[i], "-port") == 0)
				udp_port = (unsigned short) atoi(argv[++i]);
		    else
		    {
		        cout << "Usage: " << argv[0];
		        cout << " [-port <udp_port>]" << endl;
		        return 1;
		    }
		}
	}
    // start server 
    // Create a UDP socket
    int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        std::cerr << "Error creating socket." << std::endl;
        return 1;
    }
    // Set up the server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections from any interface
    server_addr.sin_port = 0; // Set port to 0 to request a random free port
    // Bind the socket to the random port
    if (bind(udp_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding socket." << std::endl;
        close(udp_sockfd);
        return 1;
    }
    // Retrieve the assigned port number
    socklen_t len = sizeof(server_addr);
    if (getsockname(udp_sockfd, (struct sockaddr*)&server_addr, &len) == -1) {
        std::cerr << "Error getting socket name." << std::endl;
        close(udp_sockfd);
        return 1;
    }
    udp_port = ntohs(server_addr.sin_port) ;
    Cmd_Msg_T msg;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    while(true)
    {   
        usleep(100);
        switch(server_state)
        {
            
            case WAITING:
            {   cout << "**************************************************************" <<endl;
                    cout << "This is the server of IERG3310 Lab2 from : 1155199171" <<endl;
                cout << "**************************************************************" <<endl;  
                std::cout << "Waiting UDP command @ port number: " << udp_port << std::endl;
                size_t recv_len = recvfrom(udp_sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &client_addr_len);
                std::cout << "[CMD RECEIVED]: " << cmd_string[msg.cmd] << std::endl;
                if (msg.cmd==1) {
                    server_state = PROCESS_LS;
                } else if (msg.cmd==2) {
                    server_state = PROCESS_SEND;
                } else if (msg.cmd==4) {
                    server_state = PROCESS_REMOVE;
                } else if (msg.cmd==5) {
                    server_state = PROCESS_RENAME;
                } else if (msg.cmd==6) {
                    server_state = SHUTDOWN;
                } else {
                    server_state = WAITING;
                }
                break;
            }
            case PROCESS_LS:
            {          
                if (process_ls(udp_sockfd, client_addr) < 0) {
					std::cerr << " - failed to process LS command." << std::endl;
				}
                server_state = WAITING;
                break;
            }
            case PROCESS_SEND:
            {   if (process_send(udp_sockfd, client_addr, msg) < 0) {
					std::cerr << " - failed to process SEND command." << std::endl;
				} 
                server_state = WAITING;
                break;
            }
            case PROCESS_REMOVE:
            {   if (process_remove(udp_sockfd, client_addr, msg) < 0) {
					std::cerr << " - failed to process REMOVE command." << std::endl;
				}             
		        server_state = WAITING;
                break;
            }
            case PROCESS_RENAME:
            {   // do  
                char filename[PATH_MAX] = "./backup/";
                char new_filename[PATH_MAX] = "./backup/";
                strcat(filename, msg.filename);
                filename[PATH_MAX - 1] = '\0';
                size_t recv_len = recvfrom(udp_sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &client_addr_len);
                strcat(new_filename, msg.filename);
                new_filename[PATH_MAX - 1] = '\0';
                if (process_rename(udp_sockfd, client_addr, filename, new_filename) < 0) {
					std::cerr << " - failed to process RENAME command." << std::endl;
				}
		        server_state = WAITING;
                break;
            }
            case SHUTDOWN:
            {   if (process_shutdown(udp_sockfd, client_addr) < 0) {
					std::cerr << " - failed to process SHUTDOWN command." << std::endl;
                    server_state = WAITING;
                    break;
				}   
                return 0;
            }
            default:
            {
           		server_state = WAITING;
                break;
            }
        }
    }
    close(udp_sockfd);
    return 0;
}

//this function check if the backup folder exist
int checkDirectory (string dir)
{
	DIR *dp;
	if((dp  = opendir(dir.c_str())) == NULL) {
        //cout << " - error(" << errno << ") opening " << dir << endl;
        if(mkdir(dir.c_str(), S_IRWXU) == 0){
            cout<< " - Note: Folder "<<dir<<" does not exist. Created."<<endl;
        }else {
            cout << " - Note: Folder " << dir << " does not exist. Cannot create. Error: " << strerror(errno) << endl;
            return errno;} // Return error code if creation fails}
        return 0; // Return 0 if the directory was created successfully
    }
    closedir(dp);
    return 0; // Return 0 if the directory was created successfully
}


//this function is used to get all the filenames from the
//backup directory
int getDirectory (string dir, vector<string> &files)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        //cout << " - error(" << errno << ") opening " << dir << endl;
        if(mkdir(dir.c_str(), S_IRWXU) == 0)
            cout<< " - Note: Folder "<<dir<<" does not exist. Created."<<endl;
        else
            cout<< " - Note: Folder "<<dir<<" does not exist. Cannot created."<<endl;
        return errno;
    }

    int j=0;
    while ((dirp = readdir(dp)) != NULL) {
    	//do not list the file "." and ".."
        if((string(dirp->d_name)!=".") && (string(dirp->d_name)!=".."))
        	files.push_back(string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}
//this function check if the file exists
bool checkFile(const char *fileName)
{
    ifstream infile(fileName);
    return infile.good();
}

