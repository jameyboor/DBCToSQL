/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/


#ifndef DBCSTRUCT_H
#define DBCSTRUCT_H

#include <string> 
#include <vector>
#include "dbcmember.h"

class DBCStruct
{
public:
	DBCStruct(std::string name);
	void AddMember(DBCMember member);

	void SetName(std::string name);
	std::string GetName();
	std::vector<DBCMember> GetMembers();
	
private:
	std::string name_;
	std::vector<DBCMember> members_;
};

#endif

