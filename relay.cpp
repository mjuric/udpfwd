/*
	Userspace UDP relay. Think of it as poor man's single-port UDP DNAT/SNAT.
*/

#include <stdio.h>          // Needed for printf()
#include <stdlib.h>         // Needed for memcpy()
#include <string.h>         // Needed for strcpy()

#include <sys/types.h>    // Needed for system defined identifiers.
#include <netinet/in.h>   // Needed for internet address structure.
#include <sys/socket.h>   // Needed for socket(), bind(), etc...
#include <arpa/inet.h>    // Needed for inet_ntoa()
#include <sys/time.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>

typedef unsigned long long uint64;

int gc_interval = 60; // connection _may_ be dropped if there was no activity within gc_interval,
		      // and _will_ be dropped if there was no activity within 2*gc_interval
std::string status_file = "status.txt";	// status file -- will be updated every gc_interval seconds with status info
std::string log_file = "log.txt"; // log file

int shutdown_interval = 120; // shutdown if there's no activity in shutdown_interval seconds (set to 0 to disable)

time_t time_of_start = time(NULL);

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

	bool dirty;
protected:
	void touch() { dirty = true; }

public:
	uint64 total_received;	// bytes received
	uint64 total_sent;	// bytes sent

	endpoint dest;		// destination for this socket (intranet side)
	endpoint source;	// source for this socket (internet side)

	unsigned int sock;		// the socket handle (for sending/receiving to intranet side)
	unsigned short sock_port;	// port to which sock is bound
	connection *const proxy;	// socket on the other side of the relay (for forwarding to the internet side)

	connection(connection *proxy_ = NULL)
		: sock(0), proxy(proxy_), from_len(sizeof(sockaddr_in)), total_sent(0), total_received(0),
		  dirty(true)
		{ }

	bool clear_dirty() { bool d = dirty; dirty = false; return d; }

	bool initialize(const endpoint &dest_, const endpoint &source_, uint16_t port_to_bind = 0)
	{
		dest = dest_;
		source = source_;
		sock_port = port_to_bind;

		sock = bind_socket(sock_port);
		touch();

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
			touch();
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
		touch();
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
		touch();
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
			ss
			   << (std::string)inet_ntoa(source.addr.sin_addr) << ":" << (int)ntohs(source.addr.sin_port)
			   << "  <-->  ["
			   << sock_port
			   << "]  <--> "
			   << (std::string)inet_ntoa(dest.addr.sin_addr) << ":" << (int)ntohs(dest.addr.sin_port)
			   ;
		} else {
			ss
			   << (std::string)inet_ntoa(source.addr.sin_addr) << ":" << (int)ntohs(source.addr.sin_port)
			   << "  <-->  ["
			   << proxy->sock_port
		           << ":"
			   << sock_port
			   << "]  <-->  "
			   << (std::string)inet_ntoa(dest.addr.sin_addr) << ":" << (int)ntohs(dest.addr.sin_port)
			;
		}
		ss << " [out=" << total_sent << "B, in=" << total_received << "B]";
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

void ctrlc_signal_handler(int sig);
void timer_signal_handler(int sig);

class relay_socket;
static relay_socket *rs = NULL;

class relay_socket : public connection
{
private:
	std::map<endpoint, connection*> connections;
	
	// source -> connection map
	typedef std::map<endpoint, connection*>::iterator con_iter;

	// statistics collection
	int n_past_connections;
	uint64 total_past_bytes_sent;     // sum of bytes sent on prior, now _closed_, connections
	uint64 total_past_bytes_received; // sum of bytes received on prior, now _closed_, connections

protected:
	int build_fdset(fd_set &set)
	{
		FD_ZERO(&set);

		int max_socket = 0;
		FOREACH(connections)
		{
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
	relay_socket()
		: connection(), should_gc(true), ctrlc(false), n_past_connections(0), total_past_bytes_sent(0), total_past_bytes_received(0)
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

	std::ofstream logstrm;
	bool open_log_stream(const std::string &fn)
	{
		logstrm.open(fn.c_str(), std::ios::app);
//		logstrm.rdbuf()->pubsetbuf(0, 0); // unbuffered logging
		logstrm << std::unitbuf; // unbuffered logging
		return logstrm.good();
	}
	
	std::ostream &log()
	{
		//return std::cerr << "[" << timestamp() << "] ";
		return logstrm << "[" << timestamp() << "] ";
	}

	connection *open_connection(const endpoint &source)
	{
		std::auto_ptr<connection> rcon(new connection(this));
		if(!rcon->initialize(dest, source)) { return NULL; }

		log() << "New: " << rcon->stats() << "\n";

		connection *conn = rcon.release();
		add_to_socket_lists(conn);

		update_status();
		return conn;
	}

	void drop_connection(const endpoint &e)
	{
		connection *c = connections[e];
		log() << "Dropping: " << c->stats() << "\n";
		
		total_past_bytes_received += c->total_received;
		total_past_bytes_sent += c->total_sent;
		n_past_connections++;

		connections.erase(e);
		delete c;

		update_status();
	}

	void shutdown()
	{
		while(!connections.empty())
		{
			if(connections.begin()->second == this)
			{
				connections.erase(connections.begin());
			}
			else
			{
				drop_connection(connections.begin()->first);
			}
		}
	}

	void print_statistics()
	{
		FOREACH(connections)
		{
			std::cerr << i->second->stats() << "\n";
		}
	}

	std::string timestamp()
	{
		time_t t = time(NULL);
		std::string s = ctime(&t);
		return s.substr(0, s.size()-1);
	}

	double calc_v(uint64 ds, time_t dt)
	{
		return (double)ds / (double)dt;
	}

	void update_status(bool log = false)
	{
		std::ofstream out(status_file.c_str());

		out << timestamp() << "\n";
		out << "Listening on port " << sock_port << ", forwarding to " << dest.getHost() << ":" << dest.getPort() << ", pid=" << getpid() << "\n";
		out << n_past_connections << " past connections, " <<  connections.size()-1 << " active connections.\n\n";

		out << "Server: " << stats() << "\n\n";

		FOREACH(connections)
		{
			if(i->second == this) continue;

			out << i->second->stats() << "\n";
		}

		uint64 total_sent = total_past_bytes_sent;
		uint64 total_received = total_past_bytes_received;
		FOREACH(connections)
		{
			total_sent += i->second->total_sent;
			total_received += i->second->total_received;
		}

		out << "\n";
		time_t ut = time(NULL) - time_of_start;
		out << "Uptime:          " << ut << " seconds\n";
		out << "Total received:  " << total_received << " bytes\n";
		out << "Total sent:      " << total_sent << " bytes\n";

		// calculate transfer rates
		static uint64 last_in = 0, last_out = 0, last_t = 0;
		time_t now = time(NULL);
		if(last_t)
		{
			double vin = calc_v(total_received - last_in, now - last_t) / 1024.;
			double vout = calc_v(total_sent - last_out, now - last_t) / 1024.;
			out << "\nTransfer rates in the past " << now - last_t << " seconds: "
				<< std::setw(4) << vin << "KB/s in, "
				<< std::setw(4) << vout << "KB/s out.\n";
		}
		last_in = total_received; last_out = total_sent; last_t = now;

		if(log)
		{
			// transfer rates in gc_interval interval
			std::stringstream ss;
			static uint64 last_in = 0, last_out = 0, last_t = 0;
			if(last_t)
			{
				double vin = calc_v(total_received - last_in, now - last_t) / 1024.;
				double vout = calc_v(total_sent - last_out, now - last_t) / 1024.;
				ss << ", "
					<< std::setw(4) << vin << "KB/s in, "
					<< std::setw(4) << vout << "KB/s out";
			}
			last_in = total_received; last_out = total_sent; last_t = now;

			this->log() << "Uptime " << ut << "s, " << connections.size()-1 << " connections, "
				<< total_received << "B in, " << total_sent << "B out"
				<< ss.str() << "\n";
		}
	}

	bool should_gc;
	void garbage_collect()
	{
		should_gc = false;

		std::vector<endpoint> to_delete;
		FOREACH(connections)
		{
			if(!i->second->clear_dirty() && i->second->proxy != NULL)
			{
				to_delete.push_back(i->first);
			}
		}

		FOREACH(to_delete)
		{
			drop_connection(*i);
		}

		update_status(true);

//		print_statistics();
	}

	int setup()
	{
		if(!open_log_stream(log_file))
		{
			std::cerr << "Cannot open log file " << log_file << " for appending. Exiting.\n";
			return -1;
		}

		log() << "Starting UDP relay, pid=" << getpid() << "\n";
		log() << "Relaying from " << sock_port << " to " << dest.getHost() << ":" << dest.getPort() << "\n";

		rs = this;
		add_to_socket_lists(this);

		if(signal(SIGINT, ctrlc_signal_handler))
		{
			log() << "Could not set the SIGINT signal handler. Exiting.\n";
			return -1;
		}

		if(signal(SIGALRM, timer_signal_handler) == SIG_ERR)
		{
			log() << "Could not set the SIGALRM signal handler. Exiting.\n";
			return -1;
		}

		itimerval itv = { { gc_interval, 0 }, { gc_interval, 0 } };
		if(setitimer(ITIMER_REAL, &itv, NULL) != 0)
		{
			log() << "Could not create garbage-collection timer. Exiting.\n";
			return -1;
		}
		return 0;
	}

	bool ctrlc;
	int listen()
	{
		// setup garbage collector, etc.
		int ret = setup();
		if(ret != 0) { return ret; }

		timeval to = {shutdown_interval, 0}, timeout = to; // timeout (in seconds, miliseconds)
		// block on sockets list until a packet is received
		fd_set set;
		time_t last_statistics = 0;
		int nset = -1;
		while(nset != 0 && !ctrlc)
		{
			if(should_gc) { garbage_collect(); }

			int max_socket = build_fdset(set);
			nset = select(max_socket+1, &set, NULL, NULL, shutdown_interval == 0 ? NULL : &timeout);
			if(nset < 0)
			{
				//std::cerr << timeout.tv_sec << "|" << timeout.tv_usec << "\n";
				if(errno == EINTR) { continue; } // the timer has fired
				break;
			}

			if(nset)
			{
				FOREACH(connections)
				{
					if(FD_ISSET(i->second->sock, &set))
					{
						i->second->receive();
					}
				}
			}
			
			timeout = to;
		}

		if(ctrlc)
		{
			log() << "Received an interrupt signal (SIGINT). Shutting down.\n";
			return 0;
		}

		if(nset == 0)
		{
			log() << "No activity for shutdown_interval (" << shutdown_interval << " seconds). Shutting down.\n";
			return 0;
		}

		if(nset < 0)
		{
			log() << "Error while doing select(): " << strerror(errno) << ". Exiting abnormally.\n";
			return -1;
		}

		return -1;
	}

	~relay_socket()
	{
		shutdown();
/*		FOREACH(connections)
		{
			if(i->second == this) continue;
			delete i->second;
		}*/
	}
};

void timer_signal_handler(int sig)
{
	if(!rs) return;
	rs->should_gc = true;
}

void ctrlc_signal_handler(int sig)
{
	if(!rs) return;
	rs->ctrlc = true;
	//(void) signal(SIGINT, SIG_DFL);
}

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

	// Create socket on which we'll listen for incoming packets
	relay_socket relay;
	if(!relay.initialize(endpoint(dest_ip, dest_port), endpoint(), listen_port))
	{
		std::cerr << "Error binding to port " << listen_port << strerror(errno) << "\n";
	}

	// start listening
	return relay.listen();
}
