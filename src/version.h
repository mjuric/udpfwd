/***************************************************************************
 *   Copyright (C) 2008 by Mario Juric                                     *
 *   mjuric@ias.edu                                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License, Version 2, as   *
 *   published by the Free Software Foundation.                            *
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
