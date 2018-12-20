/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/


#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>

class Logger
{
public:
	Logger(std::string filename);
	~Logger();

	void LogError(std::string message);
	void LogInfo(std::string message);

private:
	void Log(std::string message);

private:
	std::fstream logFile_;
};

#endif