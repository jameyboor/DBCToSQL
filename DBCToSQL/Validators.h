/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/


#ifndef VALIDATORS_H
#define VALIDATORS_H

#include "validator.h"
#include <string>
#include <regex>

namespace Validators
{
	class StringValidator : public Validator<std::string>
	{
	public:
		bool Validate(std::string str) override
		{
			for (std::string const& valid : validStrings_)
			{
                size_t foundPos = str.find(valid);
				if (foundPos != std::string::npos && foundPos == 0)
					return true;
			}

			return false;
		}

	protected:
		void AddValid(std::string str)
		{
			validStrings_.push_back(str);
		}

		std::vector<std::string> validStrings_;

	};

	class RegexValidator : public Validator<std::string>
	{
	public:
		bool Validate(std::string str) override
		{
			for (std::regex const& regex : validRegexes_)
			{
				if (std::regex_match(str, regex))
					return true;
			}

			return false;
		}

	protected:
		void AddRegex(std::regex regex)
		{
			validRegexes_.push_back(regex);
		}

		std::vector<std::regex> validRegexes_;
	};

	/* STOCK VALIDATORS */

	class MemberTypeValidator : public StringValidator
	{
	public:
		MemberTypeValidator()
		{
			validStrings_.push_back("uint8");
			validStrings_.push_back("uint16");
			validStrings_.push_back("unit32"); // For typo in parsed text file
			validStrings_.push_back("uint32");
			validStrings_.push_back("int32");
			validStrings_.push_back("char*");
			validStrings_.push_back("char const*");
			validStrings_.push_back("float");
		}
	};

	class MemberNameValidator : public RegexValidator
	{
	public:
		MemberNameValidator()
		{
			validRegexes_.push_back(std::regex("[A-Za-z0-9]+.*"));
		}
	};



}

#endif