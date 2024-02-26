#pragma once

#include <cstdint>

namespace n2ajl
{

// these are defined as 1 transmission unit for each UTF type
// they do NOT equal one full codepoint, only a minimum representation of storage
// i.e. one codepoint may consist of up to 4 UTF-8 units

using utf8_t = char;	// defined as char for compatibility (characters under 0x7F are ASCII)
using utf16_t = uint16_t;
using utf32_t = uint32_t;

class UTF8Iterator
{
public:
	UTF8Iterator(const utf8_t* str)
	{
		units = (const uint8_t*)str;
		end = units;

		while (*end) { end++; } // naive byte array length

		// ignore BOM at the start of string
		if (units + 3 <= end &&
			units[0] == 0xEF &&
			units[1] == 0xBB &&
			units[2] == 0xBF)
			units += 3;

		pos = 0;
		n = 0;
		last_read = nullptr;
	}

	utf32_t Read()
	{
		if (units >= end) { return 0; }

		utf32_t codepoint = *units; // 4 bytes holds a grapheme cluster
		if (codepoint & 0x80) // if leading bit is 1 it is unicode, if not, it's ascii
		{
			if (units != last_read)
				n = ReadCodepointBytes();

			if (!n) { return 0; }

			// if we're advancing more than 1 byte, check bounds
			if (units + n > end) // if this is hit, bad segmentation (missing bytes)
				return 0;

			// now stitch together the codepoint, use the remaining bits of the first byte
			codepoint &= ~(uint32_t(-1) << (7 - n));
			codepoint <<= (n - 1) * 6; // shift remaining bits up to make room for continue bytes

			switch (n) // unrolled loop
			{
				case 4:
					if (units[3] >> 6 != 0b10) { return 0; }
					codepoint |= (units[3] & 0x3F) << (n - 4) * 6;
				case 3:
					if (units[2] >> 6 != 0b10) { return 0; }
					codepoint |= (units[2] & 0x3F) << (n - 3) * 6;
				case 2:
					if (units[1] >> 6 != 0b10) { return 0; }
					codepoint |= (units[1] & 0x3F) << (n - 2) * 6;
					break;
				default:
					return 0;
			}

			if (codepoint > 0x10FFFF) // cannot be larger than this...
				return 0;
		}
		else
		{
			n = 1;
		}

		last_read = units;
		return codepoint;
	}

	bool Advance()
	{
		if (units >= end) { return false; }

		if (units != last_read)
			n = ReadCodepointBytes();

		if (!n) { return false; }

		if (units + n > end) // if this is hit, bad segmentation (missing bytes)
			return false;

		units += n;
		pos++;

		return true;
	}

	size_t GetNumBytesLeft() const { return end - units; }
	size_t GetPosition() const { return pos; }
	size_t GetCodepointBytes() const { return n; }
	const utf8_t* GetReadPtr() const { return (const utf8_t*)units; }

private:
	// returns zero if the codepoint is malformed
	size_t ReadCodepointBytes() const
	{
		utf32_t codepoint = *units; // 4 bytes holds a grapheme cluster
		size_t n = 1; // number of units for this cluster

		if (codepoint & 0x80) // if leading bit is 1 it is unicode, if not, it's ascii
		{
			// get cluster size from leading bits
			// if we have a valid leading byte, proceed with decoding the rest, else advance pointer and skip
			if 		(codepoint >> 5 == 0b110) 	{ n = 2; }
			else if	(codepoint >> 4 == 0b1110) 	{ n = 3; }
			else if	(codepoint >> 3 == 0b11110)	{ n = 4; }
			else 								{ n = 0; } // real unicode first byte must indicate a size of > 1
		}

		return n;
	}

	const uint8_t* units;
	const uint8_t* end;
	const uint8_t* last_read;
	size_t pos;
	size_t n;
};

class UTF16Iterator
{
public:
	UTF16Iterator(const utf16_t* str) : units(str), end(units)
	{
		while (*end) { end++; } // naive byte array length

		// determine endianness of the string (also advance 1 char)
		if (units < end && *(units++) == 0xFFFE) { is_swapped = true; } // swapped endianness
	}

	utf32_t Advance(size_t* advance = nullptr)
	{
		if (advance) { *advance = 0; }

		if (units >= end) { return 0; }

		uint16_t surrogate = *units;
		if (is_swapped) { surrogate = endian_swap(surrogate); }

		utf32_t codepoint = surrogate; // 4 bytes holds a grapheme cluster

		// we allow unpaired surrogates...
		if (codepoint >> 11 == 0x1B) // check for a surrogate pattern of 11011
		{
			// if the 11th bit is zero (first surrogate with 110110 pattern)
			bool is_upper = !(codepoint & (1 << 10));

			codepoint &= 0x3FF; // mask to first 10 bits

			// ...and we have a next unit, try processing the second surrogate
			if (is_upper && units + 1 < end)
			{
				uint32_t lower = units[1];

				if (is_swapped) { lower = endian_swap(lower); }

				if (lower >> 10 == 0x37) // if second surrogate with 110111 pattern
				{
					codepoint = (codepoint << 10) | (lower & 0x3FF) | (1 << 16);
					units++; // advance one additional surrogate

					if (advance) { (*advance)++; }
				}
			}
		}

		units++; // advance one surrogate

		if (advance) { (*advance)++; }

		return codepoint <= 0x10FFFF ? codepoint : 0;
	}

private:
	static inline utf16_t endian_swap(utf16_t i) { return ((i & 0xFF) << 8) | ((i >> 8) & 0xFF); }

	const utf16_t* units;
	const utf16_t* end;
	bool is_swapped{false};
};

inline size_t UTF32ToUTF8(utf32_t codepoint, utf8_t bytes[4])
{
	for (size_t i = 0; i < 4; i++)
		bytes[i] = '\0';

	if (codepoint < 0x80)
	{
		bytes[0] = codepoint;
		return 1;
	}

	size_t num = 1;
	if (codepoint >= 0x10000) 		{ num = 4; }
	else if (codepoint >= 0x800) 	{ num = 3; }
	else if (codepoint >= 0x80) 	{ num = 2; }

	for (size_t i = 0; i < num - 1; i++)
	{
		bytes[i] = 0b10000000 | (codepoint & 0b111111);
		codepoint >>= 6;
	}

	bytes[num - 1] = (0b11110000 << (4 - num)) | ((0b1111111 >> num) & codepoint);

	return num;
}

}