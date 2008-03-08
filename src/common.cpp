#include <iostream>
#include <sstream>
#include <netdb.h>

#include "common.h"

in_addr_t gethostbyname_or_die(const std::string &host)
{
	hostent *he = gethostbyname(host.c_str());
	if(he)
	{
		return ((in_addr*)(he -> h_addr_list[0]))->s_addr;
	}
	else
	{
		std::cerr << "Canot resolve hostname: " << host.c_str() << ". Exiting.\n";
		exit(-1);
	}
}

std::string str_interval(time_t dt0)
{
	std::stringstream ss;
	time_t dt = dt0;

	const int year = 3600*24*365;
	const int month = 3600*24*30;
	const int day = 3600*24;
	const int hour = 3600;
	const int minute = 60;
	
	int y = dt / year; dt -= y*year;
	if(y) { ss << y << " year" << (y != 1 ? "s" : "") << " "; }
	
	int m = dt / month; dt -= m*month;
	if(m) { ss << m << " month" << (m != 1 ? "s" : "") << " "; }

	int d = dt / day; dt -= d*day;
	if(d) { ss << d << " day" << (d != 1 ? "s" : "") << " "; }

	int h = dt / hour; dt -= h*hour;
	if(h) { ss << h << " hr" << (h != 1 ? "s" : "") << " "; }

	m = dt / minute; dt -= m*minute;
	if(m) { ss << m << " minute" << (m != 1 ? "s" : "") << " "; }
	
	if(dt || dt0==0) { ss << dt << " second" << (dt != 1 ? "s" : "") << " "; }

	return ss.str();
}

std::string addrportfmt(in_addr &addr, unsigned short port, bool left_align)
{
	char buffer1[30];
	snprintf(buffer1, sizeof(buffer1), "%s:%d", inet_ntoa(addr), (int)ntohs(port));

	char buffer[30];
	snprintf(buffer, sizeof(buffer), left_align ? "%-21s" : "%21s", buffer1);
	return buffer;
}

///// option_reader implementation
std::string option_reader::cmdline_usage(const std::string &progname) const
{
	std::stringstream ss;
	ss << progname << " " << help();
	return ss.str();
}

std::string option_reader::help() const
{
	std::ostringstream args, opts;
	FOREACH(params)
	{
		switch((*i)->type)
		{
		case arg:
			args << "<" << (*i)->name << "> ";
			break;
		case optparam:
			opts << "[--" << (*i)->name << "="; (*i)->write_val(opts); opts << "] ";
			break;
		}
	}
	return opts.str() + args.str();
}

bool option_reader::process(int argc, char **argv)
{
	int iarg = 0;
	for(int i = 1; i != argc; i++)
	{
		std::string arg = argv[i];

		if(arg.find("--") == 0)
		{
			// process option
			bool opt_found = false;
			FOREACH(params)
			{
				if((*i)->type != optparam) { continue; }

				std::string opt = (*i)->name + "=";
				//std::cerr << opt << " =?= " << arg << "\n";
				if(arg.find(opt, 2) != 2) { continue; }

				std::string val = arg.substr(2+opt.size());
				std::istringstream ss(val);

				(*i)->read_val(ss);
				if(!ss)
				{
					std::cerr << "Error reading option --" << (*i)->name << ".\n";
					return false;
				}
				opt_found = true;
				break;
			}
			
			if(!opt_found)
			{
				std::cerr << "Unknown option " << arg << ".\n";
				return false;
			}
		}
		else
		{
			// process argument
			iarg++;
			if(iarg > nargs)
			{
				std::cerr << "Error, too many arguments.\n";
				return false;
			}
			
			int k = 0;
			FOREACH(params)
			{
				if((*i)->type != option_reader::arg) { continue; }
				k++;
				if(k != iarg) { continue; }

				std::istringstream ss(arg);
				(*i)->read_val(ss);
				if(!ss)
				{
					std::cerr << "Error reading argument <" << (*i)->name << ">.\n";
					return false;
				}
				break;
			}
		}
	}

	if(iarg != nargs)
	{
		std::cerr << "Insufficient number of arguments (" << iarg << "/" << nargs << ").\n";
		return false;
	}
	
	return true;
}

option_reader::~option_reader()
{
	FOREACH(params)
	{
		delete *i;
	}
}
