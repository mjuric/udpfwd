#ifndef version_h__
#define version_h__

#include <iostream>
#include <stdlib.h>

static const char SATOOLS_VERSION[] = "1.0rc1 ($Format:%ad, %an <%ae>, [%H]$)";

inline void PRINT_VERSION_IF_ASKED(int argc, char **argv)
{
	for(int i=1; i < argc; i++)
		if(!strcmp(argv[i],"--version"))
		{
			std::cerr << "Version: " << SATOOLS_VERSION << "\n";
			exit(0);
		}
}

#endif // version_h__
