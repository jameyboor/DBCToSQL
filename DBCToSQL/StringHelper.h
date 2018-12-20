/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/

#ifndef STRINGHELPER_H
#define STRINGHELPER_H

#include <algorithm>
#include <vector>
#include <string>
#include <sstream>

class StringHelper
{
public:
	static std::vector<std::string> GetSegments(std::string str);
	static std::string RemoveSpaces(std::string str);
	static std::string RemoveCharacter(std::string str, char c);
};

#endif STRINGHELPER_H
