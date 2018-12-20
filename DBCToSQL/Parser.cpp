/* Copyright (C) Squall-WoW - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Fredrik/Fractional <>, August 2018
*/


#include "Parser.h"

#include <iostream>
#include <algorithm>

#include "StringHelper.h"
#include "Validators.h"

using namespace Validators;

Parser::Parser() 
	: logger_("log.txt") 
{
	addFailedDBCStruct("AchievementCriteriaEntry");
}

bool Parser::Parse(std::string fileName)
{
	structFile_ = std::ifstream(fileName);

	if (!structFile_.is_open())
		return false;

	std::string structName;
	while (NextStruct(structName))
	{
		DBCStruct dbc(structName);
		currentStruct_ = structName;
		logger_.LogInfo("###### Parsing " + structName + " ######");

		DBCMember member;
		while (NextMember(member))
			dbc.AddMember(member);

		dbcStructs_.push_back(std::move(dbc));
	}

	structFile_.close();
	return true;
}

std::vector<DBCStruct> Parser::GetDBCStructs()
{
	return dbcStructs_;
}

std::vector<std::string> Parser::GetFailedDBCStructNames()
{
	return failedDBCStructNames_;
}

void Parser::SkipBlockComments()
{
	do
	{
		CommentType commentType = ParseCommentType(currentLine_);
		if (commentType != CommentType::BLOCK_START || commentType != CommentType::BLOCK_END)
			return;

		if (commentType == CommentType::BLOCK_START)
		{
			while (ReadLine())
			{
				if (ParseCommentType(currentLine_) == CommentType::BLOCK_END)
				{
					ReadLine();
					return;
				}
			}
		}

		if (commentType == CommentType::BLOCK_END)
		{
			ReadLine();
			return;
		}
	} while (ReadLine());
}

bool Parser::NextStruct(std::string& name)
{
	while (ReadLine())
	{
		SkipBlockComments();

		std::string structHead = ParseStructHead(currentLine_);
		if (structHead.size() > 0)
		{
			name = structHead;
			return true;
		}
	}

	return false;
}

bool Parser::NextMember(DBCMember& member)
{
	while (ReadLine())
	{
		if (currentLine_ == "{")
			ReadLine();
		
		if (currentLine_ == "};")
			return false;

		SkipBlockComments();
		ConsumeSpecialMembers();

		DBCMember temp = ParseMember(currentLine_);
		if (temp.name != "")
		{
			member = temp;
			return true;
		}
	}
	return false;
}

bool Parser::ReadLine()
{
	std::string aux;
	if (std::getline(structFile_, aux))
	{
		currentLine_ = aux;
		return true;
	}

	return false;
}

CommentType Parser::ParseCommentType(std::string line)
{
	line = StringHelper::RemoveSpaces(line);

	std::string singleComment = "//";
	if (line.find("//") == 0)
		return CommentType::SINGLE;

	if (line.find("/*") == 0)
		return CommentType::BLOCK_START;

	if (line.find("*/") == 0)
		return CommentType::BLOCK_END;

	return CommentType::NONE;
}

std::string Parser::ParseStructHead(std::string line)
{
	std::vector<std::string> segments = StringHelper::GetSegments(line);
	std::string lineNoSpaces = StringHelper::RemoveSpaces(line);

	if (segments.size() < 2)
		return "";

	std::string structId = "struct";
	int pos = lineNoSpaces.find(structId);
	if (pos != 0)
		return "";

	std::smatch match;
	if (std::regex_match(segments[1], match, std::regex("[A-Za-z0-9]*")) && match[0].length() > 0)
		return match[0];

	return "";
}

DBCMember Parser::ParseMember(std::string line)
{
	DBCMember member;
	std::vector<std::string> segments = StringHelper::GetSegments(line);

	// A member consists at least of a datatype and a name;
	if (segments.size() < 2)
	{
		logger_.LogInfo("Invalid size for line segment: " + line);
		return member;
	}

	int typeIndex(0), nameIndex(1);

	// USED
	member.used = true;
	int pos = segments[0].find("//");
	if (segments[0].size() > 1 && pos != std::string::npos)
	{
		if (pos + 2 != segments[0].size())
			segments[0] = segments[0].substr(pos + 2);
		else {
			typeIndex++;
			nameIndex++;
		}
		
		member.used = false;
	}

	// TYPE
	// Handle special case for char* const
	if (segments[typeIndex] == "char" && segments[typeIndex + 1] == "const*")
		member.dataType = "char const*";
	else
	{
		MemberTypeValidator typeValidator;
		if (!typeValidator.Validate(segments[typeIndex]))
		{
			if (std::regex_search(StringHelper::RemoveSpaces(line), std::regex("/{2}[0-9]+.*")))
			{
				logger_.LogError("Missing data type: " + line);
				addFailedDBCStruct(currentStruct_);
			} 
			else
				logger_.LogInfo("Invalid data type for line=" + line);
			
			return member;
		}

		member.dataType = segments[typeIndex];
	}
	
	member.entriesCount = ParseMemberEntries(line);

	// NAME
	std::string name = segments[nameIndex];
	MemberNameValidator nameValidator;
	if (!nameValidator.Validate(name))
	{
		logger_.LogInfo("Invalid name for line=" + line);
		return member;
	}

	member.name = ParseMemberName(name);

	return member;
}

void Parser::ConsumeSpecialMembers()
{
	if (currentLine_.find("union") != std::string::npos)
	{
		while (currentLine_.find("}") == std::string::npos)
			ReadLine();
	}
	else if (currentLine_.find("struct") != std::string::npos)
	{
		while (currentLine_.find("}") == std::string::npos)
			ReadLine();
	}
}

int Parser::ParseMemberEntries(std::string line)
{
	int leftBracketPos = line.find_first_of("[");
	int rightBracketPos = line.find_first_of("]");

	if (leftBracketPos != std::string::npos && rightBracketPos != std::string::npos)
	{
		std::string sizeString = line.substr(leftBracketPos + 1, rightBracketPos - leftBracketPos - 1);
		if (std::regex_match(sizeString, std::regex("[0-9]*")))
			return std::stoi(sizeString);
	}
	else
	{
		return 1;
	}

	std::string remaining = line.substr(rightBracketPos + 1, line.size() - rightBracketPos - 1);
	
	int afterCommentPos = remaining.find_first_of("//") + 2;
	std::string sizeString = StringHelper::RemoveSpaces(remaining.substr(afterCommentPos));

	int startEntry = -1;
	int endEntry = -1;
	
	// Find the start entry
	int pos = sizeString.find_first_of("-");
	if (pos != std::string::npos)
		startEntry = std::stoi(sizeString.substr(0, pos));

	// Find the end entry
	sizeString = sizeString.substr(pos + 1, sizeString.size() - pos - 1);
	std::smatch match;
	if (std::regex_search(sizeString, match, std::regex("[0-9]+")))
	{
		int size = std::stoi(match[0]);
		if (startEntry == -1)
			return size;

		endEntry = size;
	}
	
	return endEntry - startEntry + 1;
}

std::string Parser::ParseMemberName(std::string name)
{
	name = StringHelper::RemoveCharacter(name, ';');

	//Get rid of bracket
	int leftBracketPos = name.find('[');
	if (leftBracketPos != std::string::npos)
		name = name.substr(0, leftBracketPos);

	return name;
}

void Parser::addFailedDBCStruct(std::string name)
{
	if (std::find(failedDBCStructNames_.begin(), failedDBCStructNames_.end(), name) != failedDBCStructNames_.end())
		return;

	failedDBCStructNames_.push_back(name);
}
