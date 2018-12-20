/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/


#include "StringHelper.h"

std::vector<std::string> StringHelper::GetSegments(std::string str)
{
	std::vector<std::string> segments;
	std::string aux;
	
	std::stringstream stream(str);
	while (std::getline(stream, aux, ' '))
		if (aux != "")
			segments.push_back(aux);

	return segments;
}

std::string StringHelper::RemoveSpaces(std::string str)
{
	return RemoveCharacter(str, ' ');
}

std::string StringHelper::RemoveCharacter(std::string str, char c)
{
	str.erase(std::remove(str.begin(), str.end(), c), str.end());
	return str;
}
