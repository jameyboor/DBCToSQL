/* Copyright (C) - All Rights Reserved
* Written by Jamey/Jameyboor <>, August 2018
*/


#include "DBC.h"
#include "Parser.h"

#include <algorithm>
#include <iostream>
#include <filesystem>
#include <string_view>
#include <sstream>

namespace fs = std::filesystem;

void replaceAll(std::string& str, const std::string& from, const std::string& to) 
{
	if (from.empty())
		return;

	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) 
    {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}

template<class OutIter>
OutIter write_escaped(std::string const& s, OutIter out) 
{
	*out++ = '"';
	for (std::string::const_iterator i = s.begin(), end = s.end(); i != end; ++i) {
		unsigned char c = *i;
		if (' ' <= c and c <= '~' and c != '\\' and c != '"') {
			*out++ = c;
		}
		else {
			*out++ = '\\';
			switch (c) {
				case '"':  *out++ = '"';  break;
				case '\\': *out++ = '\\'; break;
				case '\t': *out++ = 't';  break;
				case '\r': *out++ = 'r';  break;
				case '\n': *out++ = 'n';  break;
				default:
					char const* const hexdig = "0123456789ABCDEF";
					*out++ = 'x';
					*out++ = hexdig[c >> 4];
					*out++ = hexdig[c & 0xF];
			}
		}
	}
	*out++ = '"';
	return out;
}

int main()
{
    std::string folder = ".";
    std::cout << "Enter the directory you want to have DBCs converted from.\nLeave empty to use this directory:\n";
    std::getline(std::cin, folder);

    if (folder.empty())
        folder = '.';

    if (!fs::exists(folder))
    {
        std::cerr << "Directory " << folder << " not found.";
        std::cin.get();
        return 0;
    }

	Parser parser;
	parser.Parse("DBCStructure.h.txt");
	std::cout << "--------------Starting DBC extraction--------------\n";

	auto structs = parser.GetDBCStructs();

	uint32_t countFailedStructs = 0;

	std::ofstream sqlFile{ "data.sql" };


	for (auto& p : fs::directory_iterator(folder))
	{
        if (p.path().extension() != ".dbc")
            continue;

		DBC dbc{ p.path().string(), p.path().filename().string() };
		auto filename = dbc.GetName();
		auto result = dbc.Process();

        if (result != DbcReadResult::Success)
        {
            std::cout << "Failed processing " << p.path().filename().string() << ".\n";
            continue;
        }

        std::cout << "Done processing DBC file, now writing SQL.\n";

		//use this DBC and the parsed information to create a file

		std::string stripped_filename = filename;
		replaceAll(stripped_filename, "_", "");
		std::transform(stripped_filename.begin(), stripped_filename.end(), stripped_filename.begin(), ::tolower);

		std::string_view view{ stripped_filename };
		
		//first remove .dbc from the filename
		view.remove_suffix(4);


		//now check if the name matches any of the struct
		DBCStruct* entry = nullptr;
		for (auto& structEntry : structs)
		{
			std::string structName = structEntry.GetName();
			std::transform(structName.begin(), structName.end(), structName.begin(), ::tolower);
			if (structName.find(view) != std::string::npos)
			{
				entry = &structEntry;
				break;
			}
		}

		if (!entry)
		{
			std::cout << "Could NOT find struct entry for DBC " << filename << " This DBC will still be converted but no column names will be generated." << std::endl;
			++countFailedStructs;
		}

		const auto& stringTable = dbc.GetStringTable();

		std::string tableName{ view };

		decltype(entry->GetMembers()) structMembers;
        if (entry)
            structMembers = entry->GetMembers();

		const auto& rows = dbc.GetRows();

		if (!rows.size())
			continue;


		std::ostringstream table_creation;
		table_creation << "CREATE TABLE IF NOT EXISTS `" << tableName << "`  (\r\n";

		//first count the parsed members, if they dont add up to the amount of columns of the DBC that means it wasnt a good parse.
		//In that case we will still output data but not a format string nor column names.

		uint32_t memberCount = 0;
		for (const auto members : structMembers)
		{
			memberCount += members.entriesCount;
		}

		std::vector<std::string> columnNames;
		columnNames.reserve(rows[0].size());
		if (memberCount != rows[0].size() || tableName == "light" || !entry) // light.dbc is messy.
		{
			//wrong parser info, we're on our own here.
			for (uint32_t i = 0; i < rows[0].size(); ++i)
			{
				columnNames.push_back(std::string("unk") + std::to_string(i));
			}
		}
		else
		{
			uint32_t colIndex = 0;
			uint32_t currentArrayIndex = 0;
			for (uint32_t i = 0; i < rows[0].size(); ++i)
			{
				columnNames.push_back(structMembers[colIndex].name + "_" + std::to_string(currentArrayIndex));

				if (structMembers[colIndex].entriesCount > 1)
				{
					++currentArrayIndex;
					if (currentArrayIndex == structMembers[colIndex].entriesCount)
					{
						++colIndex;
						currentArrayIndex = 0;
					}
				}
				else
					++colIndex;
			}
		}


		table_creation << "`ID` INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,\n"; // add a custom auto-increment identifier, some tables have duplicate rows.
		uint32_t nameIndex = 0;
		for (const auto& cols : rows[0])
		{
			if (cols.first == ColumnType::String)
				table_creation << "`" << columnNames[nameIndex] << "` " << "TEXT NOT NULL,";

			if (cols.first == ColumnType::Int)
				table_creation << "`" << columnNames[nameIndex] << "` " << "INT(11) NOT NULL,";

			if (cols.first == ColumnType::Float)
				table_creation << "`" << columnNames[nameIndex] << "` " << "FLOAT NOT NULL,";

			if (cols.first == ColumnType::UInt)
				table_creation << "`" << columnNames[nameIndex] << "` " << "INT(11) UNSIGNED NOT NULL,";

			table_creation << "\r\n";
			++nameIndex;
		}

		table_creation << "PRIMARY KEY (`ID`)\r\n)\nCOLLATE=\'utf8_general_ci\'\r\nENGINE = InnoDB\r\n;\r\n\r\n";

		sqlFile << "START TRANSACTION;\r\n";
		sqlFile << table_creation.str();

        int32_t newQueryCounter = 0;
        
        const size_t rowLength = rows.size();
        size_t count = 0;
        size_t percentCount = 0;
        const double percentual = rowLength / 100;
		for (const auto& row : rows)
		{
            auto percent = count / percentual;
            if (static_cast<size_t>(percent) % 10 != 0 && percentCount != static_cast<size_t>(percent) / 10)
            {
                std::cout << "%";
                percentCount = percent / 10;
            }

            std::ostringstream ss;
            if (newQueryCounter == 0)
            {
                ss << "INSERT INTO `" << tableName << "` (";
                for (const auto& name : columnNames)
                {
                    ss << "`" << name << "`, ";
                }
                ss.seekp(-2, ss.cur); // remove the last comma
                ss << ") VALUES\r\n";
            }

            ss << "(";

			for (const auto& cols : row)
			{
				if (cols.first == ColumnType::String)
				{
					//we have to escape.
					std::string escaped_string = stringTable.find(std::get<uint32_t>(cols.second))->second;
					replaceAll(escaped_string, "\\", "\\\\");
					replaceAll(escaped_string, "\'", "\'\'");
					ss << "\'" << escaped_string << "\',";
				}

				if (cols.first == ColumnType::Int)
					ss << std::get<int>(cols.second) << ",";

				if (cols.first == ColumnType::UInt)
					ss << std::get<uint32_t>(cols.second) << ",";

				if (cols.first == ColumnType::Float)
					ss << std::get<float>(cols.second) << ",";
			}
			ss.seekp(-1, ss.cur); // remove the last comma

            if (newQueryCounter == -1)
            {
                ss << ");\r\n";
                newQueryCounter++;
            }
            else
            {
                ++newQueryCounter;
                ss << "),\r\n";
                if (newQueryCounter >= 249)
                    newQueryCounter = -1;
            }

			sqlFile << ss.str();
            ++count;
		}
        std::cout << '\n';

        sqlFile.seekp(-4, sqlFile.cur); // remove comma and newline to add last ; for COMMIT.
		sqlFile << ";\r\nCOMMIT;\r\n\r\n";
	}

	std::cout << "Unnamed tables : " << countFailedStructs << std::endl;
    std::cout << "Done.";
	std::cin.get();
}