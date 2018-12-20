/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/

#ifndef COLUMN_H
#define COLUMN_H

#include <string>

struct StructMember
{
	std::string name;
	std::string dataType;
	int entriesCount;
	bool used;
};

#endif