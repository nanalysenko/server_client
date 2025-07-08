#include <sys/types.h>
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
#include <netdb.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <thread>
#include <future>
#include <atomic>
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <utility>
#include <chrono> 




#include "message.h"
#include "client.h"



// using namespace std;

void sendFile(struct sockaddr_in& server_addr, char filename[FILE_NAME_LEN], int port) {
    int tcp_sockfd;
	Cmd_Msg_T msg;
	socklen_t addr_len = sizeof(server_addr);
    // Create TCP socket
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sockfd < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return;
    }

    server_addr.sin_port = htons(port); // Use htons for port

    // std::cerr << " - CONNECTING TO TCP - ADDRESS - PORT " << port << std::endl;

    // Connect to the server
    if (connect(tcp_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error connecting to server for file transfer: " << strerror(errno) << std::endl;
        close(tcp_sockfd);
        return;
    }

    std::cerr << "Connected successfully!" << std::endl;

    // Open the file to send
    std::ifstream infile(filename, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << " - cannot open file: " << filename << std::endl;
        close(tcp_sockfd);
        return;
    }

    char buffer[DATA_BUF_LEN];
	int segment = 0; 
    while (infile.read(buffer, DATA_BUF_LEN) || infile.gcount() > 0) {
        ssize_t bytes_sent = send(tcp_sockfd, buffer, infile.gcount(), 0);
		std::cout << "Buffer size: "<< bytes_sent<< std::endl;
		segment++;
    }
    std::cout << "Total Segment Number is: " << segment << std::endl;
    infile.close();
	close(tcp_sockfd); // Close TCP connection after sending
	return;
}

// Function to receive filenames
std::vector<std::string> receiveFilenames(int sockfd, struct sockaddr_in& server_addr, socklen_t& server_addr_len) {
    std::vector<std::string> filenames;
    Data_Msg_T data_msg;

    // Loop to receive filenames
    while (true) {
        // Clear the message structure
        memset(&data_msg, 0, sizeof(data_msg));
        
        // Receive message
        int bytes_received = recvfrom(sockfd, &data_msg, sizeof(data_msg), 0, (struct sockaddr*)&server_addr, &server_addr_len);
        if (bytes_received <= 0) {
            std::cerr << "Error receiving data or connection closed." << std::endl;
            break; // Exit loop if there's an error
        }

        // Check if the received message indicates the end of filenames
        if (strcmp(data_msg.data, "END") == 0) {
            break; // Exit loop if "END" message is received
        }

        // Store the filename
        filenames.push_back(std::string(data_msg.data));
    }

    return filenames;
}

// Helper function to check for whitespace
bool is_not_space(unsigned char ch) {
    return !std::isspace(ch);
}

std::string strip_whitespace(const std::string& input) {
    std::string result = input;

    // Remove leading spaces
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), is_not_space));

    // Remove trailing spaces
    result.erase(std::find_if(result.rbegin(), result.rend(), is_not_space).base(), result.end());

    return result;
}

int get_file_size(Cmd_Msg_T& msg) {
    std::ifstream file(msg.filename, std::ios::binary); // Open file in binary mode
    if (!file) {
        msg.error = 1; // Set an error code
        return -1; // Indicate failure
    }
    file.seekg(0, std::ios::end); // Move to the end of the file
    msg.size = htonl(file.tellg()); // Get the file size
	file.seekg(0, std::ios::beg); // Move back to the beginning of the file
    file.close(); // Close the file
    return 0; // Indicate success
}

// Function implementation

int process_ls(int sockfd, const struct sockaddr_in& server_addr) {
	std::vector<std::string> filenames;
    Data_Msg_T data_msg;
    Cmd_Msg_T msg;
    msg.cmd = CMD_LS;
    // Send message
    ssize_t sent_bytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (sent_bytes < 0) {
        std::cerr << " - send failed: " << strerror(errno) << std::endl;
        return -1; // Indicate failure
    }
    // Receive message
    socklen_t server_addr_len = sizeof(server_addr);
    ssize_t recv_bytes = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, &server_addr_len);
	// Handle the response
    if (recv_bytes < 0) {
        std::cerr << " - receive failed: " << strerror(errno) << std::endl;
        return -1; // Indicate failure
    } else if (msg.cmd == CMD_LS && msg.size == 0) {
        std::cout << " - server backup folder is empty." << std::endl;
    } else if (msg.cmd == CMD_LS && msg.size > 0) {
        std::cout << "Server contains " << ntohl(msg.size) << " files:" << std::endl;
		// Loop to receive filenames
		for (int i = 0; i < ntohl(msg.size); ++i) {
			memset(&data_msg, 0, sizeof(data_msg));
			int bytes_received = recvfrom(sockfd, &data_msg, sizeof(data_msg), 0, (struct sockaddr*)&server_addr, &server_addr_len);
			if (bytes_received <= 0) {
				std::cerr << "Error receiving data or connection closed." << std::endl;
				break; // Exit loop if there's an error
			}
			filenames.push_back(std::string(data_msg.data));
		}
		// print file names
		for (const auto& filename : filenames) {
        	std::cout << " - " << filename << std::endl;
    	}
	} else if(msg.cmd != CMD_LS) {
		std::cout << " - command response error." << std::endl;
	}
	return 0; // Indicate success
}

int process_send(int sockfd, struct sockaddr_in& server_addr, const std::string& in_cmd, int udp_port) {
	std::ifstream file;
	Cmd_Msg_T msg;
	char filename[FILE_NAME_LEN];
	socklen_t addr_len = sizeof(server_addr);
	// save filename
	strncpy(filename, in_cmd.c_str(), FILE_NAME_LEN-1);
	filename[FILE_NAME_LEN - 1] = '\0';
	// prepare message
	strncpy(msg.filename, in_cmd.c_str(), FILE_NAME_LEN-1);
	msg.filename[FILE_NAME_LEN - 1] = '\0';
	msg.cmd = 2;
	msg.error = htons(0);
	if (get_file_size(msg) == 0) {
		std::cout << " - filesize: " << htonl(msg.size) << " bytes" << std::endl;
	} else {
		std::cerr << " - cannot open file: " << filename << std::endl;
		file.close();
		return 0;
	}
	// send filename, filesize
	ssize_t sent_bytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (sent_bytes < 0) {
		std::cerr << " - send failed: " << strerror(errno) << std::endl;
		close(sockfd);
		return -1;
	}
	// get response from the server 
	ssize_t recv_bytes = recvfrom(sockfd, &msg, sizeof(msg), 0, 
								(struct sockaddr*)&server_addr, &addr_len);
	if(recv_bytes< 0) {
		std::cerr << " - receive failed." << std::endl;
		return -1;
	}
	// file exists
	if (ntohs(msg.error)==2) {
		std::cerr << " - file exists. overwrite? (y/n):" << std::endl;
		char userInput;
		std::cin >> userInput;
		if (userInput == 'Y' || userInput == 'y') {
			// Send normal message to overwrite
			std::cerr << " - overwrite." << std::endl;
			// send error 0 message
			msg.cmd = 2;
			msg.error =htonl(0);
			sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
			recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, &addr_len);
		} else {
			// Send error message to indicate not overwriting
			msg.cmd = 2;
			msg.error = htons(2);
			sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
			return -1;
		}
	} 
	if((msg.port==0)||(msg.cmd!=CMD_SEND)||(msg.error==1)){
		std::cout << " - error or incorrect response from server." << std::endl;
		return 0;
	} 
	int port = htons(msg.port);
	std::cout << " - TCP port: " << port << std::endl;
	// Set up server address for UDP

    // Test TCP connection
    sendFile(server_addr, filename, port); // You may want to use udp_port here instead

	return 0; 
}

int process_remove(int sockfd, struct sockaddr_in& server_addr, const std::string& in_cmd) {
    Cmd_Msg_T rm_msg;
    rm_msg.cmd = CMD_REMOVE; // COMMAND_REMOVE, for example
    std::string stripped_cmd = strip_whitespace(in_cmd); // Assuming this function exists
    strncpy(rm_msg.filename, stripped_cmd.c_str(), FILE_NAME_LEN - 1);
    rm_msg.filename[FILE_NAME_LEN - 1] = '\0'; // Ensure null termination
    rm_msg.port = ntohs(server_addr.sin_port);

    // Send message
    ssize_t sent_bytes = sendto(sockfd, &rm_msg, sizeof(rm_msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (sent_bytes < 0) {
        std::cerr << "- send failed: " << strerror(errno) << std::endl;
        return -1; // Indicate failure
    }

    // Receive message
    socklen_t server_addr_len = sizeof(server_addr);
    ssize_t recv_bytes = recvfrom(sockfd, &rm_msg, sizeof(rm_msg), 0, (struct sockaddr*)&server_addr, &server_addr_len);
    if (recv_bytes < 0) {
        std::cerr << "- receive failed: " << strerror(errno) << std::endl;
        return -1; // Indicate failure
    }

	if(ntohs(rm_msg.error) == 1) {
		std::cout<< " - file does not exist." << std::endl;
	} else {
		std::cout<< " - file is removed." << std::endl;
	}

    return 0; // Indicate success
}

int process_rename(int sockfd, struct sockaddr_in& server_addr, char filenames[FILE_NAME_LEN]){
	Cmd_Msg_T msg;
	socklen_t addr_len = sizeof(server_addr);
	// Variables to hold the separated file names
	char file1[FILE_NAME_LEN];
	char file2[FILE_NAME_LEN];
	// Find the position of the space separating the two file names
	char* spacePos = strchr(filenames, ' ');
	if (spacePos != nullptr) {
		// Calculate the length of the first file name
		size_t length1 = spacePos - filenames;
		// Copy the first file name
		strncpy(file1, filenames, length1);
		file1[length1] = '\0'; // Null-terminate the first file name
		// Copy the second file name
		strcpy(file2, spacePos + 1); // Copy from the character after the space
	} else {
		std::cerr << "- error: incorrect file-name format." << std::endl;
		return -1;
	}
	// prepare message to send filename to change
	msg.cmd = 5;
	strncpy(msg.filename, file1, FILE_NAME_LEN-1);
	msg.filename[FILE_NAME_LEN-1] = '\0';
	msg.error = 0;
	// repeat this logic in server
	ssize_t sent_bytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (sent_bytes < 0) {
		std::cerr << " - send failed: " << strerror(errno) << std::endl;
		close(sockfd);
		return -1;
	}
	msg.cmd = 5;
	strncpy(msg.filename, file2, FILE_NAME_LEN-1);
	msg.filename[FILE_NAME_LEN-1] = '\0';
	msg.error = 1;
	sent_bytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (sent_bytes < 0) {
		std::cerr << " - send failed: " << strerror(errno) << std::endl;
		close(sockfd);
		return -1;
	}
	ssize_t recv_bytes = recvfrom(sockfd, &msg, sizeof(msg), 0, 
								(struct sockaddr*)&server_addr, &addr_len);
	if (recv_bytes < 0) {
		std::cerr << " - receive failed: " << strerror(errno) << std::endl;
		close(sockfd);
		return -1;
	}
	if (ntohs(msg.error)==1) {
		std::cerr << " - file doesn’t exist." << std::endl;
		return -1;
	} else {
		std::cerr << " - file has been renamed." << std::endl;
	}
	return 0;

}

int process_shutdown(int sockfd, struct sockaddr_in& server_addr) {
	Cmd_Msg_T msg;
	msg.cmd = 6;
	msg.error = 0;
	ssize_t sent_bytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (sent_bytes < 0) {
		std::cerr << " - send failed: " << strerror(errno) << std::endl;
		close(sockfd);
		return -1;
	}
	// Receive message
    socklen_t server_addr_len = sizeof(server_addr);
    ssize_t recv_bytes = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, &server_addr_len);
	if(msg.error == 0) {
		std::cerr << " - server is shutdown."<< std::endl;
		return 0; 
	} else {
		return -1; 
	}

}

int main(int argc, char *argv[])
{
    unsigned short udp_port = 0;
    const char* server_host = "127.0.0.1";
	int sockfd;
    struct sockaddr_in server_addr;
	socklen_t addr_len;
	char buffer[1024];

    //process input arguments
	if ((argc != 3) && (argc != 5))
	{
		cout << "Usage: " << argv[0];
		cout << " [-address <server_host>] -port <udp_port>" << endl;
		return 1;
	}
	else
	{
		//system("clear");
		for (int i = 1; i < argc; i++)
		{				
			if (strcmp(argv[i], "-port") == 0)
				udp_port = (unsigned short) atoi(argv[++i]);
			else if (strcmp(argv[i], "-address") == 0)
			{
				server_host = argv[++i];
				if (argc == 3)
				{
				    cout << "Usage: " << argv[0];
		            cout << " [-address <server_host>] -port <udp_port>" << endl;
		            return 1;
				}
		    }
	        else
	        {
	            cout << "Usage: " << argv[0];
		        cout << " [-address <server_host>] -port <udp_port>" << endl;
		        return 1;
	        }
		}
	}
	// connect to server 
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd<0){
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(udp_port);
	inet_pton(AF_INET, server_host, &server_addr.sin_addr);
	

	// length of server address
	addr_len = sizeof(server_addr);
	// Print server address and port
    std::cout << "Sending to " << server_host << " on port " << udp_port << "..." << std::endl;
	std::cout << "**************************************************************" << std::endl;
	std::cout << "Successfully connect to the server! Please type your command" << std::endl;
	std::cout << "**************************************************************" << std::endl;

	Client_State_T client_state = WAITING;
	string in_cmd;
	while(true)
	{
	    usleep(100);
	    switch(client_state)
	    {
	        case WAITING:
	        {
	            cout<<"$ ";
	            cin>>in_cmd;
	            
	            if(in_cmd == "ls")
	            { 
					client_state = PROCESS_LS;
	            }
	            else if(in_cmd == "send")
	            {
					std::getline(std::cin, in_cmd);
	                client_state = PROCESS_SEND;
	            }
	            else if(in_cmd == "remove")
	            {	std::getline(std::cin, in_cmd);
	                client_state = PROCESS_REMOVE;
	            }
	            else if(in_cmd == "rename")
                {	std::getline(std::cin, in_cmd);
                    client_state = PROCESS_RENAME;
                }
	            else if(in_cmd == "shutdown")
	            {
	                client_state = SHUTDOWN;
	            }
	            else if(in_cmd == "quit")
	            {
	                client_state = QUIT;
	            }
	            else
	            {
	                cout<<" - wrong command."<<endl;
	                client_state = WAITING;
	            }
	            break;
	        }
	        case PROCESS_LS:
	        {  	if (process_ls(sockfd, server_addr) < 0) {
					std::cerr << " - failed to process LS command." << std::endl;
					client_state = WAITING;
					break;
				}
				client_state = WAITING;
				break;
	        }
	        case PROCESS_SEND:
	        {	
				in_cmd = strip_whitespace(in_cmd);
				if (process_send(sockfd, server_addr, in_cmd, htons(udp_port)) < 0) {
					std::cerr << " - failed to process SEND command." << std::endl;
					client_state = WAITING;
					break;
				}
				sockfd = socket(AF_INET, SOCK_DGRAM, 0);
				if (sockfd<0){
					perror("Socket creation failed");
					exit(EXIT_FAILURE);
				}
				server_addr.sin_family = AF_INET;
    			server_addr.sin_port = htons(udp_port);
				inet_pton(AF_INET, server_host, &server_addr.sin_addr.s_addr);
				client_state = WAITING;
				break;
				
	        }
	        case PROCESS_REMOVE:
	        {	// Send message
				if (process_remove(sockfd, server_addr, in_cmd) < 0) {
					std::cerr << " - failed to process REMOVE command." << std::endl;
				}
				client_state = WAITING;
				break;
	        }
	        case PROCESS_RENAME:
	        {	
				char filenames[FILE_NAME_LEN];
				// save filename
				in_cmd = strip_whitespace(in_cmd);
				strncpy(filenames, in_cmd.c_str(), FILE_NAME_LEN-1);
				filenames[FILE_NAME_LEN-1] = '\0';
				if (process_rename(sockfd, server_addr, filenames) < 0) {
					std::cerr << " - failed to process RENAME command." << std::endl;
				}
				client_state = WAITING;
				break;
	        }	
	        case SHUTDOWN:
	        {	
	            if (process_shutdown(sockfd, server_addr) < 0) {
					std::cerr << " - failed to process SHUTDOWN command." << std::endl;
				}
				client_state = WAITING;
				break;            
	        }
	        case QUIT:
	        {
				close(sockfd);
    			return 0;  
	        }
	        default:
	        {
	        	client_state = WAITING;
	            break;
	        }    
	    }
	}
	close(sockfd);
    return 0;
}






