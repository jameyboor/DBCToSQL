/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/


#include "dbcstruct.h"

DBCStruct::DBCStruct(std::string name) : name_(name) { }

void DBCStruct::AddMember(DBCMember member)
{
	members_.push_back(member);
}

void DBCStruct::SetName(std::string name)
{
	name_ = name;
}

std::string DBCStruct::GetName()
{
	return name_;
}

std::vector<DBCMember> DBCStruct::GetMembers()
{
	return members_;
}
