#include <iostream>
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
