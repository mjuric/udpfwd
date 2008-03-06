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
