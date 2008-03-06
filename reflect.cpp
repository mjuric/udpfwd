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

//
// Inspired by Ken Christensen's udprelay.c
//

#include <arpa/inet.h>
#include <errno.h>

#include <iostream>
#include <string>

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		std::cerr << "UDP packet reflector -- returns received packets to their senders.\n";
		std::cerr << "Usage: " << argv[0] << " <port_to_listen_on>\n";
		return -1;
	}

	const unsigned short	listen_on_port = atoi(argv[1]);

	// Bind to port on which to listen
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(listen_on_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	unsigned int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(bind(sock, (sockaddr *)&server_addr, sizeof(server_addr)) != 0)
	{
		std::cerr << "Error binding to UDP socket: " << strerror(errno) << "\n";
		return -1;
	}

	// Prepare sender address struct
	sockaddr_in   sender_addr;
	sender_addr.sin_family = AF_INET;
	socklen_t addr_len = sizeof(sender_addr);

	// Reflect or relay UDP datagrams forever
	std::cout << "Listening and reflecting packets on port " << listen_on_port << "\n";
	while(true)
	{
		// Receive
		char buffer[30000];
		int length = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr *)&sender_addr, &addr_len);

		// Diagnostics (note: assumes the packet contents is textual)
		std::string msg(buffer, 0, std::min(length, 30));
		if(length > 30) { msg += "..."; }
		std::cout << inet_ntoa(sender_addr.sin_addr) << ":" 
			<< ntohs(sender_addr.sin_port) << " --> :" << listen_on_port << " [\"" << msg << "\"]"
			<< " :" << listen_on_port << " --> "
			<< inet_ntoa(sender_addr.sin_addr) << ":" 
			<< ntohs(sender_addr.sin_port) << "\n";

		// Reflect
		sendto(sock, buffer, length, 0, (sockaddr *)&sender_addr, sizeof(sender_addr));
	}

	close(sock);
}
