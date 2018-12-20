#pragma once

//ByteConverter = TC

#include <algorithm>
#include <stdint.h>

#include "Endianness.h"

namespace ByteConverter
{
	template<size_t T>
	inline void convert(char *val)
	{
		std::swap(*val, *(val + T - 1));
		convert<T - 2>(val + 1);
	}

	template<> inline void convert<0>(char *) { }
	template<> inline void convert<1>(char *) { }           // ignore central byte

	template<typename T> inline void apply(T *val)
	{
		convert<sizeof(T)>((char *)(val));
	}
}


template<typename T> inline void EndianConvert(T& val) { if constexpr(Endian::native == Endian::big) ByteConverter::apply<T>(&val); }

template<typename T> void EndianConvert(T*);
template<typename T> void EndianConvertReverse(T*);

inline void EndianConvert(uint8_t&) { }
inline void EndianConvert(int8_t&) { }
inline void EndianConvertReverse(uint8_t&) { }
inline void EndianConvertReverse(int8_t&) { }