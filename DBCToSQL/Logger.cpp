/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/


#include "Logger.h"

Logger::Logger(std::string filename)
{
	logFile_.open(filename);
}

Logger::~Logger()
{
	logFile_.close();
}

void Logger::LogError(std::string message)
{
	Log("Error: " + message);
}

void Logger::LogInfo(std::string message)
{
	Log("Info: " + message);
}

void Logger::Log(std::string message)
{
	message += "\r\n";
	logFile_.write(message.c_str(), message.size());
}


