/* Copyright (C) - All Rights Reserved
* Written by Jamey/Jameyboor <>, August 2018
*/

#pragma once
#include "Endianness.h"

#include <stdint.h>
#include <string>
#include <fstream>
#include <vector>
#include <variant>
#include <utility>
#include <unordered_map>

#pragma pack(push, 1)
struct alignas(1) DBCHeader
{
	uint32_t magic;
	uint32_t recordCount;
	uint32_t fieldCount;
	uint32_t recordSize;
	uint32_t stringBlockSize;
};
#pragma pack(pop)


enum class DbcReadResult
{
	Success = 1,
	StreamFail = 2,
	NoDbc = 3,
	Skipped = 4
};

enum class ColumnType : uint32_t
{
	NoType = 0,
	Int = 1,
	String = 2,
	Byte = 3,
	Float = 4,
	UInt = 5
};

class DBC
{
public:
	DBC(std::string pathName, std::string fileName) : m_pathName(pathName), m_filename(fileName) {}
	
	DbcReadResult Process();

	std::string GetName() const { return m_filename; }

private:
	DbcReadResult DetectColumnTypes();
	DbcReadResult ReadHeader();
	DbcReadResult ReadData();
	DbcReadResult ReadStringTable();
	void          DetectStringTypes();
	bool          Open();

	std::string m_filename;
	std::string m_pathName;
	DBCHeader m_header;
	std::ifstream m_filestream;
	std::vector<ColumnType> m_columnTypes;
	std::unordered_map<uint32_t, std::string> stringTable; //index, string
	std::vector<std::vector<std::pair<ColumnType, std::variant<uint32_t, int32_t, float>>>> rows;

public:
	decltype(rows) GetRows() const { return rows; }
	decltype(stringTable) GetStringTable() const { return stringTable; }
};