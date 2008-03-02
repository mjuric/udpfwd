#include <stdio.h>          // Needed for printf()
#include <stdlib.h>         // Needed for memcpy()
#include <string.h>         // Needed for strcpy()

#include <sys/types.h>    // Needed for system defined identifiers.
#include <netinet/in.h>   // Needed for internet address structure.
#include <sys/socket.h>   // Needed for socket(), bind(), etc...
#include <arpa/inet.h>    // Needed for inet_ntoa()
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <iostream>
#include <string>
#include <sstream>

//===== Main program ========================================================
int main(int argc, char **argv)
{
	if(argc != 4)
	{
		std::cerr << "Usage: " << argv[0] << " <dest_ip> <dest_port> <source_port>\n";
		return -1;
	}

	std::string dest_ip = argv[1];
	uint16_t dest_port = atoi(argv[2]);
	uint16_t src_port = atoi(argv[3]);
	
	std::cerr << "Sending packets from port " << src_port << " to " << dest_ip << ":" << dest_port << "\n";

	unsigned int         server_s;      // Server socket descriptor
	struct in_addr       recv_ip_addr;  // Received IP address

	// Create my (server) socket and fill-in address information
	sockaddr_in server_addr;   // Server Internet address
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(src_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_s = socket(AF_INET, SOCK_DGRAM, 0);
	int succ = bind(server_s, (sockaddr *)&server_addr, sizeof(server_addr));
	if(succ != 0)
	{
		std::cerr << "Error binding socket: " << strerror(errno) << "\n";
		return -1;
	}

	// Address to which the datagrams will be sent
	sockaddr_in dest_addr;
	socklen_t addr_len = sizeof(dest_addr);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(dest_port);
	dest_addr.sin_addr.s_addr = inet_addr(dest_ip.c_str());

	int npacket = 0;

	// handles for select/pool

	// Send UDP packets to host
	int no_resp = 0, resp = 0;
	while(true)
	{
		// prepare the message
		std::stringstream ss; ss << "Packet #" << npacket << ".";
		std::string msg = ss.str();
		npacket++;

		// Send the datagram
		char buffer[30000] = {0};
		strcpy(buffer, msg.c_str());
		int NSEND = 18000; //msg.size()+1;
		int sent = sendto(server_s, buffer, NSEND, 0, (sockaddr *)&dest_addr, sizeof(dest_addr));
		//std::cerr << "Sent " << sent << " bytes to " << dest_ip << ":" << dest_port << " from 0.0.0.0:" << ntohs(server_addr.sin_port) << "\n";
		std::cerr << "To " << dest_ip << ":" << dest_port << " from 0.0.0.0:" << ntohs(server_addr.sin_port) << "\n";
		std::cerr << "   Message: '" << msg << "' (" << sent << " bytes sent)\n";

		// Use select to wait for the response
		timeval timeout = {1, 0}; // timeout (in seconds, miliseconds)
		fd_set set; FD_ZERO(&set); FD_SET(server_s, &set);
		succ = select(server_s+1, &set, NULL, NULL, &timeout);
		switch(succ)
		{
			case -1:
				std::cerr << "  Error while doing select(): " << strerror(errno) << "\n";
				break;
			case 0:
				std::cerr << "  No response received\n";
				no_resp++;
				break;
			default:
				//std::cerr << "select returned " << succ << "\n";
				// Get response
				resp++;
				char buffer[30000];
				sockaddr_in client_addr = {0};
				socklen_t cli_addr_len = sizeof(client_addr);
				succ = recvfrom(server_s, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &cli_addr_len);
				std::cerr << "  Response: '" <<  buffer << "' (" << succ << " bytes, from "
					  << inet_ntoa(client_addr.sin_addr) << ":" << (int)ntohs(client_addr.sin_port) << ")\n";
				break;
		}

		std::cerr << "Stats: " << resp << "/" << no_resp << "/" << resp+no_resp << "\n";
//		sleep(1);
	}

	// Close and clean-up
	close(server_s);
}
