#ifndef common_h__
#define common_h__

#include <string>
#include <arpa/inet.h>

#define FOREACHj(i_, x) for(typeof((x).begin()) i_ = (x).begin(); i_ != (x).end(); ++i_)
#define FOREACH(x) FOREACHj(i, x)

in_addr_t gethostbyname_or_die(const std::string &host);
std::string str_interval(time_t dt);
std::string addrportfmt(in_addr &addr, unsigned short port, bool left_align = 1);

#endif
