/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/

#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <fstream>
#include <vector>
#include <regex>

#include "Column.h"
#include "dbcstruct.h"
#include "dbcmember.h"
#include "Logger.h"

enum CommentType
{
	NONE,
	SINGLE,
	BLOCK_START,
	BLOCK_END
};

class Parser
{
public:
	Parser();
	bool Parse(std::string fileName);
	std::vector<DBCStruct> GetDBCStructs();
	std::vector<std::string> GetFailedDBCStructNames();
	
private:
	bool ReadLine();

	bool NextStruct(std::string&name);
	bool NextMember(DBCMember& member);
	void SkipBlockComments();
	void ConsumeSpecialMembers();

	CommentType ParseCommentType(std::string line);
	std::string ParseStructHead(std::string line);
	DBCMember ParseMember(std::string line);
	int ParseMemberEntries(std::string line);
	std::string ParseMemberName(std::string name);

	void addFailedDBCStruct(std::string name);

private:
	Logger logger_;
	std::ifstream structFile_;
	std::string currentLine_;
	std::string currentStruct_;
	std::vector<DBCStruct> dbcStructs_;
	std::vector<std::string> failedDBCStructNames_;
};

#endif 

