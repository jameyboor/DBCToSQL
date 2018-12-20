/* Copyright (C) - All Rights Reserved
* Written by Fredrik/Fractional <>, August 2018
*/


#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <string>

template<typename T>
class Validator
{
public:
	virtual bool Validate(T data) = 0; 
};

#endif

