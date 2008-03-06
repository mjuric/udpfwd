/***************************************************************************
 *   Copyright (C) 2008 by Mario Juric                                     *
 *   mjuric@ias.edu                                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <arpa/inet.h>
#include <errno.h>

#include <iostream>
#include <string>
#include <sstream>

#include "common.h"

int main(int argc, char **argv)
{
	if(argc != 4 && argc != 5)
	{
		std::cerr << "Usage: " << argv[0] << " <dest_ip> <dest_port> <source_port> [seconds_between_packets=1]\n";
		return -1;
	}

	std::string dest_ip = argv[1];
	uint16_t dest_port = atoi(argv[2]);
	uint16_t src_port = atoi(argv[3]);
	int send_delay = argc == 5 ? atoi(argv[4]) : 1;

	std::cout << "Sending packets from port " << src_port << " to " << dest_ip << ":" << dest_port << "\n";

	// Socket from which to transmit the datagrams
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(src_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(bind(sock, (sockaddr *)&server_addr, sizeof(server_addr)) != 0)
	{
		std::cerr << "Error binding to UDP socket: " << strerror(errno) << "\n";
		return -1;
	}

	// Destination address
	sockaddr_in dest_addr;
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(dest_port);
	dest_addr.sin_addr.s_addr = gethostbyname_or_die(dest_ip.c_str());

	int npacket = 0, no_resp = 0, resp = 0; // number of packets sent, responses missed, responses received
	while(true)
	{
		// Prepare message
		std::stringstream ss; ss << "Packet #" << npacket << ".";
		std::string buffer = ss.str();
		npacket++;

		// Send datagram
		int sent = sendto(sock, buffer.c_str(), buffer.size()+1, 0, (sockaddr *)&dest_addr, sizeof(dest_addr));

		// Diagnostics (note: assumes the packet contents is textual)
		std::string msg(buffer, 0, std::min((int)buffer.size(), 30));
		if(buffer.size() > 30) { msg += "..."; }
		std::cout << dest_ip << ":" << dest_port << " <-- :" << src_port << " \"" << msg << "\" (" << sent << " bytes sent)\n";

		// Wait for response
		timeval timeout = {1, 0}; // timeout (in seconds, miliseconds)
		fd_set set; FD_ZERO(&set); FD_SET(sock, &set);
		int succ = select(sock+1, &set, NULL, NULL, &timeout);
		switch(succ)
		{
			case -1:
				std::cout << "  Error while doing select(): " << strerror(errno) << " ";
				break;
			case 0:
				std::cout << "  No response ";
				no_resp++;
				break;
			default:
				// Got response
				resp++;
				char buffer[30000];
				sockaddr_in sender_addr = {0};
				socklen_t addr_len = sizeof(dest_addr);
				int length = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr *)&sender_addr, &addr_len);

				// Diagnostics (note: assumes the packet contents is textual)
				std::string msg(buffer, 0, std::min(length, 30));
				if(length > 30) { msg += "..."; }
				std::cout << inet_ntoa(sender_addr.sin_addr) << ":" 
					<< ntohs(sender_addr.sin_port) << " --> :" << src_port << " \"" << msg << "\" ";

				break;
		}

		std::cout << "[" << resp << "/" << no_resp << "/" << resp+no_resp << "]\n\n";
		sleep(send_delay);
	}

	close(sock);
}
