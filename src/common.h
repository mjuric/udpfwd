#ifndef common_h__
#define common_h__

#include <string>
#include <vector>
#include <iostream>
#include <arpa/inet.h>

#define FOREACHj(i_, x) for(typeof((x).begin()) i_ = (x).begin(); i_ != (x).end(); ++i_)
#define FOREACH(x) FOREACHj(i, x)

in_addr_t gethostbyname_or_die(const std::string &host);
std::string str_interval(time_t dt);
std::string addrportfmt(in_addr &addr, unsigned short port, bool left_align = 1);

class option_reader
{
public:
	enum { arg, optparam };
	int nargs;

protected:
	struct io
	{
		int type; // this is either option_reader::arg or option_reader::optparam
		std::string name;

		io(int type_, const std::string &name_) : type(type_), name(name_) {}

		virtual std::istream  &read_val(std::istream &in) = 0;
		virtual std::ostream &write_val(std::ostream &out) const = 0;
	};

	template<typename T>
	struct io_spec : public io
	{
		T &var;
		io_spec(T &v, int type_, const std::string &name_) : var(v), io(type_, name_) {}

		virtual std::istream  &read_val(std::istream &in)  { return  in >> var; }
		virtual std::ostream &write_val(std::ostream &out) const { return out << var; }
	};

	std::vector<io*> params;

public:
	option_reader() : nargs(0) { }

	template<typename T> void add(const std::string &name, T &t, int type);
	bool process(int argc, char **argv);

	std::string cmdline_usage(const std::string &progname) const;
	std::string help() const;
	
	~option_reader();
};

template<typename T>
void option_reader::add(const std::string &name, T &t, int type)
{
	io_spec<T> *io = new io_spec<T>(t, type, name);
	params.push_back(io);

	if(type == arg) { nargs++; }
}

#endif
