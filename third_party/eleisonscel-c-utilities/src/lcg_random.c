/* Copyright 2026 EleisonScel
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include	"lcg_random.h"

#include	"assert_m.h"

#include	<stdint.h>	/* uint64_t */

#ifndef NDEBUG
#	include	<inttypes.h>/* PRIu32 */
#endif

#if defined(_MSC_VER)
#	include <intrin.h>
#elif !defined(__has_builtin)
#	define __has_builtin(x) 0
#endif /* _MSC_VER */

static inline int8_t lcg_bit_position( uint32_t value );
static uint64_t	lcg_get_bits_64( int bits );

static uint64_t seed64 = 1;
static uint32_t seed32 = 1;

void lcg_set_random32( uint32_t seed ) {
	seed32 = seed;
}

void lcg_set_random64( uint64_t seed ) {
	seed64 = seed;
}

uint64_t lcg_random64(void) {
	const uint64_t a = 6364136223846793005ULL;
	const uint64_t b = 1442695040888963407ULL;

	seed64 = seed64 * a + b;
	return seed64;
}

uint32_t lcg_random32(void) {
	const uint32_t a = 1664525;
	const uint32_t b = 1013904223;

	seed32 = seed32 * a + b;
	return seed32;
}

uint32_t lcg_rand32_max( uint32_t max ) {
	assert_m( max != 0, "Max must be positive" );
	if ( max <= 1 ) return 0;

	if ( (max & ( max - 1 )) == 0 )
		return (uint32_t) lcg_get_bits_64((int)lcg_bit_position( max ));

	uint64_t random	= lcg_get_bits_64(32) * max;
	uint32_t left	= (uint32_t) random;

	if ( left < max ) {
		uint32_t remain = -max % max;
		while ( left < remain ) {
			random	= lcg_get_bits_64(32) * max;
			left	= (uint32_t) random;
		}
	}

	return (uint32_t)(random >> 32);
}

uint64_t lcg_rand64_max( uint64_t max ) {
	assert_m( max != 0, "Max must be positive" );
	if ( max <= 1 ) return 0;

	uint64_t max_value = UINT64_MAX;
	uint64_t distribution = max_value - ( max_value % max );

	uint64_t random_value;
	do {
		random_value = lcg_random64();
	} while( random_value >= distribution );
	return random_value % max;
}

/* Function:
 * extract exactly amount of random bits from linear congruential generator
 *
 * Parameters:
 * bits		- number of bits to extract
 *
 * Returns:
 * value	- random value within some bits
 */
static uint64_t lcg_get_bits_64( int bits ) {
	assert_mf( bits > 0 && bits <= 64, "Invalid bit count (bits: %d)", bits );

	static uint64_t	buffer		= 0;
	static int		bits_left	= 0;

	uint64_t		result		= 0;

	while ( bits > 0 ) {
		if ( bits_left == 0 ) {
			buffer		= lcg_random64();
			bits_left	= 64;
		}
		int take		= bits < bits_left ? bits : bits_left;
		uint64_t mask	= (take == 64) ? UINT64_MAX : (((uint64_t)1 << take) - 1 );
		result			= (result << take) | (buffer & mask);
		buffer			>>= take;
		bits_left		-= take;
		bits			-= take;
	}
	return result;
}

/* Function:
 * determine a bit position of a power-of-two value
 *
 * Parameters:
 * value	- value to detect a bit in
 *
 * Returns:
 * int8_t	- bit index
 */
static inline int8_t lcg_bit_position( uint32_t value ) {
	assert_mf(
		(value & (value - 1)) == 0,
		"Value must be a power-of-two to find a bit (value: %" PRIu32 ")", value
	);
	assert_m( value != 0, "Value shouldn't be zero" );

#if defined(_MSC_VER)

	unsigned long index;
	_BitScanForward( &index, value );
	return (int8_t) index;

#elif __has_builtin(__builtin_ctz)

	return (int8_t) __builtin_ctz(value);

#else

	uint32_t magic = 0x077CB531U;
	static const char bits_table[32] = {
		 0,  1, 28,  2, 29, 14, 24, 3,
		30, 22, 20, 15, 25, 17,  4, 8,
		31, 27, 13, 23, 21, 19, 16, 7,
		26, 12, 18,  6, 11,  5, 10, 9
	};

	return bits_table[(value * magic) >> 27];

#endif /* _MSC_VER */
}
