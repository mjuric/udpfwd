/*
	Userspace UDP relay.
*/

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
#include <map>

unsigned int bind_socket(unsigned short &base)
{
#define BASE 6000

	bool find_available = base == 0;
	if(find_available) { base = BASE; }

	unsigned short last = base + 5000;
	sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	unsigned int s = socket(AF_INET, SOCK_DGRAM, 0);
	// TODO: see how to flag the socket O_NONBLOCK;

	while(base < last)
	{
		sockaddr.sin_port = htons(base);
		int succ = bind(s, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
		if(succ == 0)	// found and bound a free port
		{
			int ret = fcntl(s, F_SETFL, O_NONBLOCK);	// set the socket to nonblocking
			if(ret == -1) { return 0; }
			return s;
		}

		if(!find_available || errno != EADDRINUSE) { return 0; }

		++base;
	}
	return 0;	// failed to find a free port
}

// Class encapsulating connection endpoint (address+port)
class endpoint
{
public:
	// note: these are all in network byte order
	sockaddr_in addr;

	endpoint(const std::string &host = "0.0.0.0", uint16_t port = 0)
	{
#if 0
		hostent *he = gethostbyname(host.c_str());
		if(he)
		{
			addr.sin_addr.s_addr = ((in_addr*)(he -> h_addr_list[0]))->s_addr;
		}
#endif
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = inet_addr(host.c_str());
////		std::cerr << "HHH: " << host << " : " << getHost() << "\n";
	}
	endpoint(const endpoint &r) : addr(r.addr) {}

	bool operator <(const endpoint &r) const { return addr.sin_addr.s_addr < r.addr.sin_addr.s_addr || (addr.sin_addr.s_addr == r.addr.sin_addr.s_addr && addr.sin_port < r.addr.sin_port); }

	uint16_t getPort() const { return ntohs(addr.sin_port); }
	std::string getHost() const { return inet_ntoa(addr.sin_addr); }
};

class connection
{
protected:
	char buffer[30000];
	endpoint from_tmp;
	socklen_t from_len;
	int total_received;
	int total_sent;

public:
	endpoint dest;		// destination for this socket (intranet side)
	endpoint source;	// source for this socket (internet side)

	unsigned int sock;		// the socket handle (for sending/receiving to intranet side)
	unsigned short sock_port;	// port to which sock is bound
	connection *const proxy;	// socket on the other side of the relay (for forwarding to the internet side)

	connection(connection *proxy_ = NULL) : sock(0), proxy(proxy_), from_len(sizeof(sockaddr_in)), total_sent(0), total_received(0) { }

	bool initialize(const endpoint &dest_, const endpoint &source_, uint16_t port_to_bind = 0)
	{
		dest = dest_;
		source = source_;
		sock_port = port_to_bind;

		sock = bind_socket(sock_port);

//		std::cerr << "dest=" << dest.getHost() << ":" << dest.getPort() << ", listening_on=" << sock_port << "\n";
//		std::cerr << stats() << "\n";
		return sock != 0;
	}

	~connection()
	{
		if(sock) { close(sock); }
	}

	bool receive()
	{
		// Receive datagrams on socket sock (intranet side) and forward
		// them through the proxy socket (internet side)
		while(true)
		{
			// receive
			ssize_t succ = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_tmp.addr, &from_len);
			if(succ == -1)
			{
				return errno == EAGAIN || errno == EWOULDBLOCK;
			}
			total_received += succ;
//			std::cerr << " Packet: " << succ << " Message = '" << buffer << "'\n";
//			std::cerr << "   From: " << inet_ntoa(from_tmp.addr.sin_addr) << ":" << (int)ntohs(from_tmp.addr.sin_port) << "\n";
			
			if(!forward(buffer, succ, &from_tmp)) return false;
		}
	}

	virtual bool forward(char *buffer, int len, endpoint *from = NULL)
	{
		// forward the received packet through proxy socket (internet side)
		if(!proxy || !proxy->send(buffer, len, &source))
		{
			return false;
		}
	}

	bool send(const char *buffer, int len, endpoint *dest = NULL)
	{
		// send a packet through this socket to the destination dest,
		// or the default destination (intranet side endpoint)
		if(!dest) { dest = &this->dest; }
//		std::cerr << "DDD"  << "dest=" << dest << " "
//		                   << (std::string)inet_ntoa(dest->addr.sin_addr) << ":" << (int)ntohs(dest->addr.sin_port)
//					<< this->dest.getHost()
//		                   ;
		ssize_t succ = sendto(sock, buffer, len, 0, (struct sockaddr *)&dest->addr, sizeof(dest->addr));
		if(succ != -1) { total_sent += succ; }
//		std::cerr << "Out!\n";
		return succ != -1;
	}
	
	void reset_traffic_counters()
	{
		total_received = 0;
		total_sent = 0;
	}

	std::string stats()
	{
		std::ostringstream ss;
#if 0
		ss << "source="
		   << (std::string)inet_ntoa(source.addr.sin_addr) << ":" << (int)ntohs(source.addr.sin_port) << "  -->  "
		   << ":" << sock_port << " --> "
		   << "dest="
		   << (std::string)inet_ntoa(dest.addr.sin_addr) << ":" << (int)ntohs(dest.addr.sin_port)
		   << " via proxy=" << (proxy ? proxy->stats() : "NULL")
		   ;
#endif
		if(!proxy) {
			ss << "A: "
			   << (std::string)inet_ntoa(source.addr.sin_addr) << ":" << (int)ntohs(source.addr.sin_port) << "  --  "
			   << ":" << sock_port
			   << " <---> "
			   << (std::string)inet_ntoa(dest.addr.sin_addr) << ":" << (int)ntohs(dest.addr.sin_port)
			   ;
		} else {
			ss << "A: "
			   << (std::string)inet_ntoa(source.addr.sin_addr) << ":" << (int)ntohs(source.addr.sin_port) << "  --  "
			   << ":" << proxy->sock_port
		           << " <---> "
			   << " :" << sock_port << " "
			   << (std::string)inet_ntoa(dest.addr.sin_addr) << ":" << (int)ntohs(dest.addr.sin_port)
			;
		}
		ss << " [S=" << total_sent << ", R=" << total_received << "]";
		return ss.str();
	}
};

#define FOREACHj(i_, x) for(typeof((x).begin()) i_ = (x).begin(); i_ != (x).end(); ++i_)
#define FOREACH(x) FOREACHj(i, x)

struct connection_less
{
	bool operator ()(const connection *a, const connection *b) const
	{
		return a->dest < b->dest;
	}
};

class relay_socket : public connection
{
private:
	std::map<endpoint, connection*> connections;
	
	// source -> connection map
	typedef std::map<endpoint, connection*>::iterator con_iter;

protected:
	int build_fdset(fd_set &set)
	{
		FD_ZERO(&set);

		int max_socket = 0;
		FOREACH(connections)
		{
//			std::cerr << "XX " << i->second->sock << "\n";
			FD_SET(i->second->sock, &set);
			if(max_socket < i->second->sock) { max_socket = i->second->sock; }
		}

		return max_socket;
	}

	void reset_all_traffic_counters()
	{
		FOREACH(connections)
		{
			i->second->reset_traffic_counters();
		}
	}

public:
	relay_socket() : connection()
	{
	}

	// overridden to forward the received datagrams through apropriate connections (demultiplexing)
	virtual bool forward(char *buffer, int len, endpoint *from = NULL)
	{
		// retreive or create the relay connection
		con_iter it = connections.find(*from);
		connection *rcon = it != connections.end() ? it->second : open_connection(*from);
		if(!rcon) return false;

		// forward the packet
//		std::cerr << "Going to send... " << rcon << " " << buffer << " " << len << "\n";
		return rcon->send(buffer, len);
	}

	connection *add_to_socket_lists(connection *conn)
	{
		connections[conn->source] = conn;
//		std::cerr << "Number of open connections: " << connections.size() << "\n";
//		std::cerr << "dest = " << conn->dest.getHost() << "\n";
		return conn;
	}

	connection *open_connection(const endpoint &source)
	{
		// TODO: implement garbage collection
//		std::cerr << "Opening new connection.\n";

		std::auto_ptr<connection> rcon(new connection(this));
		if(!rcon->initialize(dest, source)) { return NULL; }

		return add_to_socket_lists(rcon.release());
	}

	void print_statistics()
	{
		FOREACH(connections)
		{
			std::cerr << i->second->stats() << "\n";
		}
	}

	int listen()
	{
		add_to_socket_lists(this);


		// block on sockets list until a packet is received
		fd_set set;
		for(;;)
		{
//			reset_all_traffic_counters();

			timeval timeout = {20, 0}; // timeout (in seconds, miliseconds)
			int max_socket = build_fdset(set);

			int nset = select(max_socket+1, &set, NULL, NULL, &timeout);
			if(nset < 0) break;

//			std::cerr << "nset=" << nset << " " << timeout.tv_sec << "|" << timeout.tv_usec << " ";
//			std::cerr << "nset=" << nset << " ";
			if(nset)
			{
				FOREACH(connections)
				{
					if(FD_ISSET(i->second->sock, &set))
					{
//						std::cerr << "Triggered " << i->second->sock << ".\n";
//						goto endd;
						i->second->receive();
//						std::cerr << "Done receiving.\n";
					}
				}
			}

			print_statistics();
		}
endd:
		return 0;
	}

	~relay_socket()
	{
		FOREACH(connections)
		{
			if(i->second == this) continue;
			delete i->second;
		}
	}
};

int main(int argc, char **argv)
{
	if(argc != 4)
	{
		std::cerr << "Usage: " << argv[0] << " <forward_to_ip> <forward_to_port> <listen_at_port>\n";
		return -1;
	}

	std::string dest_ip = argv[1];
	uint16_t dest_port = atoi(argv[2]);
	uint16_t listen_port = atoi(argv[3]);
	std::cerr << "Forwarding UDP packets received at port " << listen_port << " to " << dest_ip << ":" << dest_port << "\n";

	// Create socket on which we'll listen for incoming packets
	relay_socket relay;
	if(!relay.initialize(endpoint(dest_ip, dest_port), endpoint(), listen_port))
	{
		std::cerr << "Error binding to port " << listen_port << strerror(errno) << "\n";
	}

	// start listening
	return relay.listen();
}
