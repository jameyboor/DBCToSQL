/* Copyright (C) - All Rights Reserved
* Written by Jamey/Jameyboor <>, August 2018
*/


#include "DBC.h"
#include "Byteconverter.h"

#include <vector>
#include <filesystem>
#include <string_view>
#include <iostream>
#include <algorithm>
#include <any>
#include <type_traits>
#include <random>
#include <iostream>
#include <sstream>
#include <cmath>
#include <variant>
#include <utility>
#include <unordered_map>

DbcReadResult DBC::Process()
{
	if (!Open())
		return DbcReadResult::StreamFail;

	auto headerRes = ReadHeader();

	if (headerRes != DbcReadResult::Success)
		return headerRes;

	return ReadData();
}


//For float vs int we will read a float, if its value is 1.000.000 or less than 0.01 its an int because of the way floats are stored.
//Once we know if it's an int or a float we will proceed.
//If it's a float, we know its real type, done.
//If it's an int we check if any values are indices to the stringtable, if so, it's probably a string.
//For byte detection we have to guess by looking at the recordSize and make an assumption based on that, reading byte for byte.
DbcReadResult DBC::DetectColumnTypes()
{
	static_assert(sizeof(int) == sizeof(float) && sizeof(int) == sizeof(unsigned int));

	std::mt19937 mt{ std::random_device{}() };

	auto streamPos = m_filestream.tellg();
	uint32_t recordCount = m_header.recordCount;
	uint32_t recordSize = m_header.recordSize;
	std::vector<std::vector<char>> rows;
	rows.resize(recordCount);

	if (!recordCount)
	{
		std::cout << "File " << m_filename << " has no entries, skipping." << std::endl;
		return DbcReadResult::Skipped;
	}

	std::uniform_int_distribution rowRandomizer{ static_cast<uint32_t>(0), recordCount - 1 };

	for (auto& vec : rows)
		vec.resize(recordSize);

	{
		uint32_t i = 0;

		while (i < recordCount)
		{
			m_filestream.read(&rows[i][0], recordSize);
			if (!m_filestream)
				break;
			++i;
		}
	}

	if (!m_filestream)
		return DbcReadResult::StreamFail;

	uint32_t sizeLeft = recordSize;
	std::ostringstream ss;
	//columns
	for (size_t i = 0; i < rows[0].size();)
	{
		ColumnType type = ColumnType::NoType;

		//we have to move i by the size of the column type, but we have to first figure out what the type is.

		enum HitType : uint32_t
		{
			FirstByteSet,
			SecondByteSet,
			ThirdByteSet,
			FourthByteSet,

			FirstByteUnset,
			SecondByteUnset,
			ThirdByteUnset,
			FourthByteUnset,

			ArraySize
		};

		std::vector<uint32_t> hitTypeRecorder;
		hitTypeRecorder.resize(ArraySize);

		bool byteFree = sizeLeft % sizeof(int) == 0;
		if (!byteFree) // dont handle byte dbcs for now
		{
			std::cout << "File " << m_filename << " has one or more BYTE type columns, skipping file for now." << std::endl;
			return DbcReadResult::Skipped;
		}

		std::vector<int> intRows;
		intRows.reserve(rows[0].size());
		std::vector<float> floatRows;
		floatRows.reserve(rows[0].size());
		std::vector<unsigned int> uintRows;
		uintRows.reserve(rows[0].size());
		//^ These are filled with their representation of the bytes, no matter if they make sense or not to be used later.

		//rows
		const auto siz = rows.size();
		for (size_t j = 0; j < siz; ++j)
		{
			if (sizeLeft < sizeof(int))
			{
				type = ColumnType::Byte;
				break;
			}

			if (!rows[j][i])
				++hitTypeRecorder[FirstByteUnset];
			else
				++hitTypeRecorder[FirstByteSet];

			if (!rows[j][i + 1])
				++hitTypeRecorder[SecondByteUnset];
			else
				++hitTypeRecorder[SecondByteSet];

			if (!rows[j][i + 2])
				++hitTypeRecorder[ThirdByteUnset];
			else
				++hitTypeRecorder[ThirdByteSet];

			if (!rows[j][i + 3])
				++hitTypeRecorder[FourthByteUnset];
			else
				++hitTypeRecorder[FourthByteSet];

			if (j % 4 == 0 && rows[0].size() - j >= sizeof(int))
			{

				int valInt = *reinterpret_cast<int*>(&rows[j][i]);
				intRows.push_back(valInt);
				float val = *reinterpret_cast<float*>(&rows[j][i]);
				floatRows.push_back(val);


				uint32_t valUint = *reinterpret_cast<unsigned int*>(&rows[j][i]);
				uintRows.push_back(valUint);

			}
		}

		//The exponent and decimal of an IEEE 754 floating point is stored at byte[3].
		//An uint32 or int32 starts at byte[0].
		//We must check for dissimilarities.

		const auto IsUint = [&uintRows]() -> bool
		{
			for (const auto& elem : uintRows)
			{
				if (elem > 3'000'000'000)
					return false; // fail-fast, this is underflow and thus the original number was negative.
			}
			return true;
		};



		if (!hitTypeRecorder[FirstByteSet])
		{
			//This is either a float or an int
			if (!hitTypeRecorder[FourthByteSet])
			{
				//must be an (U)int
				if (IsUint())
					type = ColumnType::UInt;
				else
					type = ColumnType::Int;
			}
			else
				type = ColumnType::Float;
		}
		else
		{
			//(U)int or byte..
			//because the remaining fieldsize is bigger or equal than an int, and we're not free of bytes. this means that we have at least :
			// sizeof(int) + sizeof(char) left to read.
			if (!byteFree)
			{
				//If this is an int, byte three and definitely four should most of the time be empty since DBCs dont store super high values.
				//If this is a byte, byte three and four are the values of the next column's byte two and three.
				//Because we have 5 bytes left, we can interpret the last 4 as a float.
				//If this float has a sensible value then this current column is probably a byte and the next is a float.
				//If it doesnt have a sensible value, then either this current column is a byte and the next is an int.
				//Or this current column is an int and we're cross-reading multiple columns, this is the worst scenario.

				float value = *reinterpret_cast<float*>(&rows[0][i + 1]); // start reading current column + byte length
				if (value > 0.001 && value < 1'000'000)
				{
					//sensible value, this column is a byte.
					type = ColumnType::Byte;
				}
				else
				{
					//nonsensible, either a byte or and next is an int, or current is an int.
					//We can check this by checking the third and fourth byte and make an educated guess.

					if (hitTypeRecorder[FourthByteSet])
					{
						//Fourth byte is never set for ints in DBC.
						//This means that we're actually reading the third byte of the next int and our current column is a byte.
						type = ColumnType::Byte;
					}
					else
					{
						if (hitTypeRecorder[ThirdByteSet] >= (recordSize / 8))
						{
							//if 12.5% or more of the third byte is set its probably the second byte of the next column and we're reading a byte.
							type = ColumnType::Byte;
						}
						else
						{
							//finally, it's most likely an (U)int.
							if (IsUint())
								type = ColumnType::UInt;
							else
								type = ColumnType::Int;
						}
					}
				}
			}
			else
			{
				//we have no extra bytes left, easy.

				for (const auto& elem : floatRows)
				{
					if (std::isnan(elem) || elem > 1'000'000 || elem < 0.01f)
					{
						if (IsUint())
							type = ColumnType::UInt;
						else
							type = ColumnType::Int;
						break;
					}
				}

				if (type == ColumnType::NoType) // all parsed successfully.
					type = ColumnType::Float;
			}
		}

		if (type == ColumnType::NoType)
		{
			std::cerr << "ERROR: We have no type for column, binary index " << i << " in file " << m_filename;
			return DbcReadResult::StreamFail;
		}

		switch (type)
		{
			case ColumnType::Byte:
				i += sizeof(char);
				sizeLeft -= sizeof(char);
				break;

				//sizeof int is the same as uint or float, so we can use that.
			default:
				i += sizeof(int);
				sizeLeft -= sizeof(int);
				break;
		}

		//NOTE: ColumnType::String is NOT set here yet. we first have to read the stringtable and match the integer value to see if it matches up -
		// with stringtable indices before we can say its a string.

		m_columnTypes.push_back(type);
	}


	m_filestream.seekg(streamPos); // reset for actual data reading.
	return DbcReadResult::Success;
}

DbcReadResult DBC::ReadHeader()
{
	std::vector<char> headerBuffer;
	headerBuffer.resize(20);
	m_filestream.read(&headerBuffer[0], headerBuffer.size());

	if (!m_filestream)
		return DbcReadResult::StreamFail;

	unsigned char bytes[4];
	DBCHeader *header = reinterpret_cast<DBCHeader*>(&headerBuffer[0]);
	uint32_t magic = header->magic;

	bytes[0] = (magic >> 24) & 0xFF;
	bytes[1] = (magic >> 16) & 0xFF;
	bytes[2] = (magic >> 8) & 0xFF;
	bytes[3] = magic & 0xFF;

	std::string res{ std::begin(bytes), std::end(bytes) };
	std::reverse(std::begin(res), std::end(res));

	if (res != "WDBC")
	{
		std::cout << "File " << m_filename << " is not a DBC file, skipping." << std::endl;
		return DbcReadResult::NoDbc;
	}

	m_header = *header;
	return DbcReadResult::Success;
}

DbcReadResult DBC::ReadData()
{
	//We first need to dynamically figure out the format of the DBC columns.
	auto res = DetectColumnTypes();

	if (res != DbcReadResult::Success)
		return res;

	uint32_t recordCount = m_header.recordCount;
	uint32_t recordSize = m_header.recordSize;
	std::vector<std::vector<char>> byteRows;
	byteRows.resize(recordCount);

	for (auto& vec : byteRows)
		vec.resize(recordSize);

	{
		uint32_t i = 0;

		while (i < recordCount)
		{
			m_filestream.read(&byteRows[i][0], recordSize);
			if (!m_filestream)
				break;
			++i;
		}
	}


	for (auto& row : byteRows)
	{
		uint32_t i = 0;
		decltype(rows)::value_type typedRow;
		typedRow.reserve(m_columnTypes.size());
		for (const auto& type : m_columnTypes)
		{
			switch (type)
			{
				case ColumnType::Float:
				{
					decltype(typedRow)::value_type::second_type var = *reinterpret_cast<float*>(&row[i]);
					typedRow.push_back({ type, var });
					i += sizeof(float);
				}break;

				case ColumnType::Int:
				{
					decltype(typedRow)::value_type::second_type var = *reinterpret_cast<int*>(&row[i]);
					typedRow.push_back({ type, var });
					i += sizeof(int);
				}break;

				case ColumnType::UInt:
				{
					decltype(typedRow)::value_type::second_type var = *reinterpret_cast<unsigned int*>(&row[i]);
					typedRow.push_back({ type, var });
					i += sizeof(unsigned int);
				}break;
			}
		}
		rows.push_back(std::move(typedRow));
	}



	//if the stringtable contains strings we'll need to fixup the columntypes to also count in strings.
	ReadStringTable();
	DetectStringTypes();

	{
		std::ostringstream ss;
		for (const auto& column : rows[0])
		{
			switch (column.first)
			{
				case ColumnType::Byte:
					ss << "b";
					break;

				case ColumnType::Float:
					ss << "f";
					break;

				case ColumnType::Int:
					ss << "i";
					break;

				case ColumnType::UInt:
					ss << "u";
					break;

				case ColumnType::String:
					ss << "s";
					break;
			}
		}
		std::string columns = ss.str();
		std::cout << "Columns for " << m_filename << " (" << columns.size() << " columns) :\n" << columns << std::endl;
	}

	return DbcReadResult::Success;
}

DbcReadResult DBC::ReadStringTable()
{
	size_t index = 0;
	do
	{
		std::string str;
		if (!std::getline(m_filestream, str, '\0'))
			break;
		stringTable[static_cast<uint32_t>(index)] = str;
		index += str.size() + 1; // +1 for '\0', null terminator, std::string doesn't take that into account.
	} while (m_filestream);
	return DbcReadResult::Success;
}

void DBC::DetectStringTypes()
{
	//every DBC has a dummy string, thats why > 1
	if (stringTable.size() > 1)
	{
		uint32_t columnIndex = 0;
		for (uint32_t columnIndex = 0; columnIndex < rows[0].size(); ++columnIndex)
		{
			auto& column = rows[0][columnIndex];
			if (column.first == ColumnType::UInt) // this might be a string column.. time to find out.
			{
				//go over each row and match the string table, if it's a match then it's a string column.
				bool string = true;
				for (const auto& row : rows)
				{
					if (stringTable.find(std::get<uint32_t>(row[columnIndex].second)) == stringTable.end()) // not found, not a string.
					{
						string = false;
						break;
					}
				}

				if (string) // all rows are indices for the table, this is a string column, loop over the rows again to change the type.
				{
					for (auto& row : rows)
					{
						row[columnIndex].first = ColumnType::String;
					}
				}
			}
		}
	}
}

bool DBC::Open()
{
	m_filestream = decltype(m_filestream){m_pathName, std::fstream::binary};
	if (m_filestream)
		return true;
	return false;
}
