/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/

#ifndef DBCMEMBER_H
#define DBCMEMBER_H

#include <string>

struct DBCMember
{
public: 
	DBCMember()
	{
		entriesCount = 1;
		used = false;
	}

	std::string name;
	std::string dataType;
	int entriesCount;
	bool used;
};

#endif

