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

/* Portable checked bitwise shifts with well-defined wrapping.
 *
 * Every sa_safe_sh<direction>_<type> function performs a shift without
 * undefined or implementation-defined behavior unlike a built-in operators
 * for signed types or excessive shift amounts.
 *
 * 1. Unsigned shifts are zero-fill.
 * 2. Signed left shifts do a bitwise truncation.
 * 3. signed right shifts are sign-extending.
 *
 * Precondition:
 * result_pointer	- isn't NULL
 *
 * Arguments:
 * value_original	- original value to shift
 * bits_shift_amount- amount of bits to shift in [0, bit width of the type - 1]
 * result_pointer	- pointer to store the shifted value
 *
 * Returns:
 * true				- failed: result_pointer is NULL, bits_shift_amount is too wide
 * false			- result saved
 */

#pragma once

#ifndef SAFE_SHIFT_H
#define SAFE_SHIFT_H

#	include "assert_m.h"	/* assert_check_m	*/

#	include <stdint.h>		/* uint8_t			*/
#	include <stddef.h>		/* size_t			*/
#	include <stdbool.h>		/* bool				*/

#	include <limits.h>		/* CHAR_BIT			*/

#	ifndef SIZE_MAX
#		error "SIZE_MAX is not defined. A C99 compliant compiler is required."
#	endif
#	ifndef PTRDIFF_MAX
#		error "PTRDIFF_MAX is not defined. A C99 compliant compiler is required."
#	endif
#	ifndef INTMAX_MAX
#		error "INTMAX_MAX is not defined. A C99 compliant compiler is required."
#	endif
#	ifndef UINTMAX_MAX
#		error "UINTMAX_MAX is not defined. A C99 compliant compiler is required."
#	endif

#	if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L 
#		if PTRDIFF_MAX < 32767
#			error "ptrdiff_t must be at least 16 bits C23"
#		endif
#	else
#		if PTRDIFF_MAX < 65535
#			error "ptrdiff_t must be at least 17 bits C99-C17"
#		endif
#	endif /* __STDC_VERSION__ == 202311L */
static_assert_m( SIZE_MAX	>= 65535u,					"size_t must be at least 16 bits"	);
static_assert_m( INTMAX_MAX	>= 0x7FFFFFFFFFFFFFFFLL,	"intmax_t must be at least 64 bits"	);
static_assert_m( UINTMAX_MAX>= 0xFFFFFFFFFFFFFFFFULL,	"uintmax_t must be at least 64 bits");

/* check if we have wide unsigned type for ptrdiff_t */
#	if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L
#		if defined(UINT64_MAX) && PTRDIFF_WIDTH == 64
#			define SA_SAFE_PTRDIFF_UNSIGNED uint64_t
#		elif PTRDIFF_WIDTH == UINT_LEAST64_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least64_t
#		elif defined(UINT32_MAX) && PTRDIFF_WIDTH == 32
#			define SA_SAFE_PTRDIFF_UNSIGNED uint32_t
#		elif PTRDIFF_WIDTH == UINT_LEAST32_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least32_t
#		elif defined(UINT16_MAX) && PTRDIFF_WIDTH == 16
#			define SA_SAFE_PTRDIFF_UNSIGNED uint16_t
#		elif PTRDIFF_WIDTH == UINT_LEAST16_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least16_t
#		elif PTRDIFF_WIDTH == UINTMAX_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uintmax_t
#		elif defined (UINTPTR_WIDTH) && PTRDIFF_WIDTH == UINTPTR_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uintptr_t
#		elif PTRDIFF_WIDTH == SIZE_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED size_t
/* thinner */
#		elif defined(UINT32_MAX) && PTRDIFF_WIDTH <= 32
#			define SA_SAFE_PTRDIFF_UNSIGNED uint32_t
#		elif PTRDIFF_WIDTH <= UINT_LEAST32_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least32_t
#		elif defined(UINT64_MAX) && PTRDIFF_WIDTH <= 64
#			define SA_SAFE_PTRDIFF_UNSIGNED uint64_t
#		elif PTRDIFF_WIDTH <= UINT_LEAST64_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least64_t
#		elif PTRDIFF_WIDTH <= UINTMAX_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uintmax_t
#		elif defined (UINTPTR_WIDTH) && PTRDIFF_WIDTH <= UINTPTR_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED uintptr_t
#		elif PTRDIFF_WIDTH <= SIZE_WIDTH
#			define SA_SAFE_PTRDIFF_UNSIGNED size_t
#		else
#			error "No unsigned match for ptrdiff_t found"
#		endif /* PTRDIFF_WIDTH */
#	else
#		if defined(INT64_MAX) && PTRDIFF_MAX == INT64_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint64_t
#		elif PTRDIFF_MAX == INT_LEAST64_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least64_t
#		elif defined(INT32_MAX) && PTRDIFF_MAX == INT32_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint32_t
#		elif PTRDIFF_MAX == INT_LEAST32_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least32_t
#		elif defined(INT16_MAX) && PTRDIFF_MAX == INT16_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint16_t
#		elif PTRDIFF_MAX == INT_LEAST16_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least16_t
#		elif PTRDIFF_MAX == INTMAX_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uintmax_t
#		elif defined(INTPTR_MAX) && PTRDIFF_MAX == INTPTR_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uintptr_t
#		elif PTRDIFF_MAX == (SIZE_MAX / 2)
#			define SA_SAFE_PTRDIFF_UNSIGNED size_t
/* thinner */
#		elif defined(INT32_MAX) && PTRDIFF_MAX <= INT32_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint32_t
#		elif PTRDIFF_MAX <= INT_LEAST32_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least32_t
#		elif defined(INT64_MAX) && PTRDIFF_MAX <= INT64_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint64_t
#		elif PTRDIFF_MAX <= INT_LEAST64_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uint_least64_t
#		elif defined(INTPTR_MAX) && PTRDIFF_MAX <= INTPTR_MAX
#			define SA_SAFE_PTRDIFF_UNSIGNED uintptr_t
#		elif PTRDIFF_MAX <= (SIZE_MAX / 2)
#			define SA_SAFE_PTRDIFF_UNSIGNED size_t
#		else
#			define SA_SAFE_PTRDIFF_UNSIGNED uintmax_t 
#		endif /* PTRDIFF_MAX */
#	endif /* __STDC_VERSION__ == 202311L */

#	if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L
#		if (defined(INT64_MAX)		&& PTRDIFF_WIDTH == 64)								||\
			(defined(INT32_MAX)		&& PTRDIFF_WIDTH == 32)								||\
			(defined(INT16_MAX)		&& PTRDIFF_WIDTH == 16)								||\
			(defined(UINTPTR_WIDTH)	&& PTRDIFF_WIDTH == UINTPTR_WIDTH)					||\
			PTRDIFF_WIDTH == UINT_LEAST64_WIDTH || PTRDIFF_WIDTH == UINT_LEAST32_WIDTH	||\
			PTRDIFF_WIDTH == UINT_LEAST16_WIDTH || PTRDIFF_WIDTH == SIZE_WIDTH			||\
			PTRDIFF_WIDTH == UINTMAX_WIDTH
#			define SA_SAFE_PTRDIFF_WIDTH_MATCHES 1
static_assert_m(
	sizeof(ptrdiff_t) == sizeof(SA_SAFE_PTRDIFF_UNSIGNED),
	"ptrdiff_t width mismatch for union type-punning"
);
#		else
#			define SA_SAFE_PTRDIFF_WIDTH_MATCHES 0
#		endif /* SA_SAFE_PTRDIFF_UNSIGNED == ... */
#	else
#		if (defined(INT64_MAX)	&& PTRDIFF_MAX == INT64_MAX)				||\
			(defined(INT32_MAX)	&& PTRDIFF_MAX == INT32_MAX)				||\
			(defined(INT16_MAX)	&& PTRDIFF_MAX == INT16_MAX)				||\
			(defined(INTPTR_MAX)&& PTRDIFF_MAX == INTPTR_MAX)				||\
			PTRDIFF_MAX == INT_LEAST16_MAX || PTRDIFF_MAX == INT_LEAST32_MAX||\
			PTRDIFF_MAX == INT_LEAST64_MAX || PTRDIFF_MAX == INTMAX_MAX		||\
			PTRDIFF_MAX == (SIZE_MAX / 2)
#			define SA_SAFE_PTRDIFF_WIDTH_MATCHES 1
static_assert_m(
	sizeof(ptrdiff_t) == sizeof(SA_SAFE_PTRDIFF_UNSIGNED), "ptrdiff_t width mismatch"
);
#		else
#			define SA_SAFE_PTRDIFF_WIDTH_MATCHES 0
#		endif /* PTRDIFF_MAX == ... */
#	endif /* __STDC_VERSION__ == 202311L */

#	if defined(INT8_MAX) && defined(UINT8_MAX)
static inline bool sa_safe_shl_int8_t(
		int8_t value_original, size_t bits_shift_amount, int8_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 8 )
		return true;

	uint8_t value_unsigned = (uint8_t)((uint8_t) value_original << bits_shift_amount);

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	union {
		int8_t	value_signed;
		uint8_t	value_unsigned;
	} result_union;

	result_union.value_unsigned = value_unsigned;
	*result_pointer = result_union.value_signed;

#		else

	if ( value_unsigned > INT8_MAX ) {
		uint8_t value_truncated = value_unsigned - ((uint8_t) INT8_MAX + 1);
		*result_pointer = (int8_t) value_truncated - INT8_MAX - 1;
	} else {
		*result_pointer = (int8_t) value_unsigned;
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INT8_MAX */

#	if defined(UINT8_MAX)
static inline bool sa_safe_shl_uint8_t(
		uint8_t value_original, size_t bits_shift_amount, uint8_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 8 )
		return true;

	*result_pointer = value_original << bits_shift_amount;
	return false;
}
#	endif /* UINT8_MAX */

#	if defined(INT16_MAX) && defined(UINT16_MAX)
static inline bool sa_safe_shl_int16_t(
		int16_t value_original, size_t bits_shift_amount, int16_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 16 )
		return true;

	uint16_t value_unsigned = (uint16_t)((uint16_t) value_original << bits_shift_amount);

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	union {
		int16_t	value_signed;
		uint16_t	value_unsigned;
	} result_union;

	result_union.value_unsigned = value_unsigned;
	*result_pointer = result_union.value_signed;

#		else

	if ( value_unsigned > INT16_MAX ) {
		uint16_t value_truncated = value_unsigned - ((uint16_t) INT16_MAX + 1);
		*result_pointer = (int16_t) value_truncated - INT16_MAX - 1;
	} else {
		*result_pointer = (int16_t) value_unsigned;
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INT16_MAX */

#	if defined(UINT16_MAX)
static inline bool sa_safe_shl_uint16_t(
		uint16_t value_original, size_t bits_shift_amount, uint16_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 16 )
		return true;

	*result_pointer = value_original << bits_shift_amount;
	return false;
}
#	endif /* UINT16_MAX */

#	if defined(INT32_MAX) && defined(UINT32_MAX)
static inline bool sa_safe_shl_int32_t(
		int32_t value_original, size_t bits_shift_amount, int32_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 32 )
		return true;

	uint32_t value_unsigned = (uint32_t)((uint32_t) value_original << bits_shift_amount);

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	union {
		int32_t	value_signed;
		uint32_t	value_unsigned;
	} result_union;

	result_union.value_unsigned = value_unsigned;
	*result_pointer = result_union.value_signed;

#		else

	if ( value_unsigned > INT32_MAX ) {
		uint32_t value_truncated = value_unsigned - ((uint32_t) INT32_MAX + 1);
		*result_pointer = (int32_t) value_truncated - INT32_MAX - 1;
	} else {
		*result_pointer = (int32_t) value_unsigned;
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INT32_MAX */

#	if defined(UINT32_MAX)
static inline bool sa_safe_shl_uint32_t(
		uint32_t value_original, size_t bits_shift_amount, uint32_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 32 )
		return true;

	*result_pointer = value_original << bits_shift_amount;
	return false;
}
#endif /* UINT32_MAX */

#	if defined(INT64_MAX) && defined(UINT64_MAX)
static inline bool sa_safe_shl_int64_t(
		int64_t value_original, size_t bits_shift_amount, int64_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 64 )
		return true;

	uint64_t value_unsigned = (uint64_t)((uint64_t) value_original << bits_shift_amount);

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	union {
		int64_t	value_signed;
		uint64_t	value_unsigned;
	} result_union;

	result_union.value_unsigned = value_unsigned;
	*result_pointer = result_union.value_signed;

#		else

	if ( value_unsigned > INT64_MAX ) {
		uint64_t value_truncated = value_unsigned - ((uint64_t) INT64_MAX + 1);
		*result_pointer = (int64_t) value_truncated - INT64_MAX - 1;
	} else {
		*result_pointer = (int64_t) value_unsigned;
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INT64_MAX */

#	if defined(UINT64_MAX)
static inline bool sa_safe_shl_uint64_t(
		uint64_t value_original, size_t bits_shift_amount, uint64_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 64 )
		return true;

	*result_pointer = value_original << bits_shift_amount;
	return false;
}
#endif /* UINT64_MAX */

static inline bool sa_safe_shl_size_t(
		size_t value_original, size_t bits_shift_amount, size_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(size_t) * CHAR_BIT )
		return true;

	*result_pointer = value_original << bits_shift_amount;
	return false;
}

static inline bool sa_safe_shl_ptrdiff_t(
		ptrdiff_t value_original, size_t bits_shift_amount, ptrdiff_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(ptrdiff_t) * CHAR_BIT )
		return true;

	SA_SAFE_PTRDIFF_UNSIGNED value_shift_result = (SA_SAFE_PTRDIFF_UNSIGNED)
		((SA_SAFE_PTRDIFF_UNSIGNED) value_original << bits_shift_amount);

#		if SA_SAFE_PTRDIFF_WIDTH_MATCHES == 0

	SA_SAFE_PTRDIFF_UNSIGNED bits_mask = ((SA_SAFE_PTRDIFF_UNSIGNED)PTRDIFF_MAX << 1) | 1;
	SA_SAFE_PTRDIFF_UNSIGNED result_mask = value_shift_result & bits_mask;

	if ( result_mask > (SA_SAFE_PTRDIFF_UNSIGNED) PTRDIFF_MAX ) {
		SA_SAFE_PTRDIFF_UNSIGNED result =
			result_mask - ((SA_SAFE_PTRDIFF_UNSIGNED) PTRDIFF_MAX + 1);
		*result_pointer = (ptrdiff_t) result - PTRDIFF_MAX - 1;
	} else {
		*result_pointer = (ptrdiff_t) result_mask;
	}

#		elif defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	union {
		ptrdiff_t					value_signed;
		SA_SAFE_PTRDIFF_UNSIGNED	value_unsigned;
	} result_union;
	result_union.value_unsigned = value_shift_result;

	*result_pointer = result_union.value_signed;

#		else

	if ( value_shift_result <= (SA_SAFE_PTRDIFF_UNSIGNED) PTRDIFF_MAX ) {
		*result_pointer = (ptrdiff_t) value_shift_result;
	} else {
		SA_SAFE_PTRDIFF_UNSIGNED result =
			value_shift_result - ((SA_SAFE_PTRDIFF_UNSIGNED) PTRDIFF_MAX + 1);
		*result_pointer = (ptrdiff_t) result - PTRDIFF_MAX - 1;
	}

#		endif /* SA_SAFE_PTRDIFF_WIDTH_MATCHES */

	return false;
}

static inline bool sa_safe_shl_intmax_t(
		intmax_t value_original, size_t bits_shift_amount, intmax_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(intmax_t) * CHAR_BIT )
		return true;

	uintmax_t value_unsigned = (uintmax_t)((uintmax_t) value_original << bits_shift_amount);

#	if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	union {
		intmax_t	value_signed;
		uintmax_t	value_unsigned;
	} result_union;

	result_union.value_unsigned = value_unsigned;
	*result_pointer = result_union.value_signed;

#	else

	if ( value_unsigned > INTMAX_MAX ) {
		uintmax_t value_truncated = value_unsigned - ((uintmax_t) INTMAX_MAX + 1);
		*result_pointer = (intmax_t) value_truncated - INTMAX_MAX - 1;
	} else {
		*result_pointer = (intmax_t) value_unsigned;
	}

#	endif /* __STDC_VERSION__ == 202311L */

	return false;
}

static inline bool sa_safe_shl_uintmax_t(
		uintmax_t value_original, size_t bits_shift_amount, uintmax_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(uintmax_t) * CHAR_BIT )
		return true;

	*result_pointer = value_original << bits_shift_amount;
	return false;
}

#	ifdef INTPTR_MAX
static inline bool sa_safe_shl_intptr_t(
		intptr_t value_original, size_t bits_shift_amount, intptr_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(intptr_t) * CHAR_BIT )
		return true;

	uintptr_t value_unsigned = (uintptr_t)((uintptr_t) value_original << bits_shift_amount);

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	union {
		intptr_t	value_signed;
		uintptr_t	value_unsigned;
	} result_union;

	result_union.value_unsigned = value_unsigned;
	*result_pointer = result_union.value_signed;

#		else

	if ( value_unsigned > INTPTR_MAX ) {
		uintptr_t value_truncated = value_unsigned - ((uintptr_t) INTPTR_MAX + 1);
		*result_pointer = (intptr_t) value_truncated - INTPTR_MAX - 1;
	} else {
		*result_pointer = (intptr_t) value_unsigned;
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INTPTR_MAX */

#	ifdef UINTPTR_MAX
static inline bool sa_safe_shl_uintptr_t(
		uintptr_t value_original, size_t bits_shift_amount, uintptr_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(uintptr_t) * CHAR_BIT )
		return true;

	*result_pointer = value_original << bits_shift_amount;
	return false;
}
#	endif /* UINTPTR_MAX */

#	if defined(UINT8_MAX) && defined(INT8_MAX)
static inline bool sa_safe_shr_int8_t(
		int8_t value_original, size_t bits_shift_amount, int8_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 8 )
		return true;

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	*result_pointer = value_original >> bits_shift_amount;

#		else

	if ( value_original >= 0 ) {
		*result_pointer = (int8_t)(value_original >> bits_shift_amount);
	} else {
		uint8_t value_unsigned = (uint8_t) value_original;
		value_unsigned = (uint8_t) ~( (uint8_t)(~value_unsigned) >> bits_shift_amount);

		if ( value_unsigned > (uint8_t) INT8_MAX ) {
			uint8_t result_truncated = value_unsigned - ((uint8_t) INT8_MAX + 1);
			*result_pointer = (int8_t) result_truncated - INT8_MAX - 1;
		} else {
			*result_pointer = (int8_t) value_unsigned;
		}
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INT8_MAX */

#	if defined(UINT8_MAX)
static inline bool sa_safe_shr_uint8_t(
		uint8_t value_original, size_t bits_shift_amount, uint8_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 8 )
		return true;

	*result_pointer = (uint8_t)(value_original >> bits_shift_amount);
	return false;
}
#	endif /* UINT8_MAX */

#	if defined(UINT16_MAX) && defined(INT16_MAX)
static inline bool sa_safe_shr_int16_t(
		int16_t value_original, size_t bits_shift_amount, int16_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 16 )
		return true;

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	*result_pointer = value_original >> bits_shift_amount;

#		else

	if ( value_original >= 0 ) {
		*result_pointer = (int16_t)(value_original >> bits_shift_amount);
	} else {
		uint16_t value_unsigned = (uint16_t) value_original;
		value_unsigned = (uint16_t) ~( (uint16_t)(~value_unsigned) >> bits_shift_amount);

		if ( value_unsigned > (uint16_t) INT16_MAX ) {
			uint16_t result_truncated = value_unsigned - ((uint16_t) INT16_MAX + 1);
			*result_pointer = (int16_t) result_truncated - INT16_MAX - 1;
		} else {
			*result_pointer = (int16_t) value_unsigned;
		}
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INT16_MAX */

#	if defined(UINT16_MAX)
static inline bool sa_safe_shr_uint16_t(
		uint16_t value_original, size_t bits_shift_amount, uint16_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 16 )
		return true;

	*result_pointer = (uint16_t)(value_original >> bits_shift_amount);
	return false;
}
#	endif /* UINT16_MAX */

#	if defined(UINT32_MAX) && defined(INT32_MAX)
static inline bool sa_safe_shr_int32_t(
		int32_t value_original, size_t bits_shift_amount, int32_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 32 )
		return true;

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	*result_pointer = value_original >> bits_shift_amount;

#		else

	if ( value_original >= 0 ) {
		*result_pointer = (int32_t)(value_original >> bits_shift_amount);
	} else {
		uint32_t value_unsigned = (uint32_t) value_original;
		value_unsigned = (uint32_t) ~( (uint32_t)(~value_unsigned) >> bits_shift_amount);

		if ( value_unsigned > (uint32_t) INT32_MAX ) {
			uint32_t result_truncated = value_unsigned - ((uint32_t) INT32_MAX + 1);
			*result_pointer = (int32_t) result_truncated - INT32_MAX - 1;
		} else {
			*result_pointer = (int32_t) value_unsigned;
		}
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INT32_MAX */

#	if defined(UINT32_MAX)
static inline bool sa_safe_shr_uint32_t(
		uint32_t value_original, size_t bits_shift_amount, uint32_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 32 )
		return true;

	*result_pointer = (uint32_t)(value_original >> bits_shift_amount);
	return false;
}
#	endif /* UINT32_MAX */

#	if defined(UINT64_MAX) && defined(INT64_MAX)
static inline bool sa_safe_shr_int64_t(
		int64_t value_original, size_t bits_shift_amount, int64_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 64 )
		return true;

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	*result_pointer = value_original >> bits_shift_amount;

#		else

	if ( value_original >= 0 ) {
		*result_pointer = (int64_t)(value_original >> bits_shift_amount);
	} else {
		uint64_t value_unsigned = (uint64_t) value_original;
		value_unsigned = (uint64_t) ~( (uint64_t)(~value_unsigned) >> bits_shift_amount);

		if ( value_unsigned > (uint64_t) INT64_MAX ) {
			uint64_t result_truncated = value_unsigned - ((uint64_t) INT64_MAX + 1);
			*result_pointer = (int64_t) result_truncated - INT64_MAX - 1;
		} else {
			*result_pointer = (int64_t) value_unsigned;
		}
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INT64_MAX */

#	if defined(UINT64_MAX)
static inline bool sa_safe_shr_uint64_t(
		uint64_t value_original, size_t bits_shift_amount, uint64_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= 64 )
		return true;

	*result_pointer = (uint64_t)(value_original >> bits_shift_amount);
	return false;
}
#	endif /* UINT64_MAX */

static inline bool sa_safe_shr_ptrdiff_t(
		ptrdiff_t value_original, size_t bits_shift_amount, ptrdiff_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(ptrdiff_t) * CHAR_BIT )
		return true;

#	if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	*result_pointer = value_original >> bits_shift_amount;

#	else

	if ( value_original >= 0 ) {
		*result_pointer = value_original >> bits_shift_amount;
	} else {
		SA_SAFE_PTRDIFF_UNSIGNED value_unsigned = (SA_SAFE_PTRDIFF_UNSIGNED) value_original;

		value_unsigned = (SA_SAFE_PTRDIFF_UNSIGNED)
			~( (SA_SAFE_PTRDIFF_UNSIGNED)(~value_unsigned) >> bits_shift_amount);

#		if SA_SAFE_PTRDIFF_WIDTH_MATCHES == 0
		SA_SAFE_PTRDIFF_UNSIGNED bits_mask = ((SA_SAFE_PTRDIFF_UNSIGNED) PTRDIFF_MAX << 1) | 1;
		value_unsigned &= bits_mask;
#		endif /* SA_SAFE_PTRDIFF_WIDTH_MATCHES */

		if ( value_unsigned > (SA_SAFE_PTRDIFF_UNSIGNED) PTRDIFF_MAX ) {
			SA_SAFE_PTRDIFF_UNSIGNED result_truncated =
				value_unsigned - ((SA_SAFE_PTRDIFF_UNSIGNED) PTRDIFF_MAX + 1);
			*result_pointer = (ptrdiff_t) result_truncated - PTRDIFF_MAX - 1;
		} else {
			*result_pointer = (ptrdiff_t) value_unsigned;
		}

	}

#	endif /* __STDC_VERSION__ == 202311L */

	return false;
}

static inline bool sa_safe_shr_intmax_t(
		intmax_t value_original, size_t bits_shift_amount, intmax_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(intmax_t) * CHAR_BIT )
		return true;

#	if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	*result_pointer = value_original >> bits_shift_amount;

#	else

	if ( value_original >= 0 ) {
		*result_pointer = (intmax_t)(value_original >> bits_shift_amount);
	} else {
		uintmax_t value_unsigned = (uintmax_t) value_original;
		value_unsigned = (uintmax_t) ~( (uintmax_t)(~value_unsigned) >> bits_shift_amount);

		if ( value_unsigned > (uintmax_t) INTMAX_MAX ) {
			uintmax_t result_truncated = value_unsigned - ((uintmax_t) INTMAX_MAX + 1);
			*result_pointer = (intmax_t) result_truncated - INTMAX_MAX - 1;
		} else {
			*result_pointer = (intmax_t) value_unsigned;
		}
	}

#	endif /* __STDC_VERSION__ == 202311L */

	return false;
}

static inline bool sa_safe_shr_uintmax_t(
		uintmax_t value_original, size_t bits_shift_amount, uintmax_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(uintmax_t) * CHAR_BIT )
		return true;

	*result_pointer = (uintmax_t)(value_original >> bits_shift_amount);
	return false;
}


#	ifdef INTPTR_MAX
static inline bool sa_safe_shr_intptr_t(
		intptr_t value_original, size_t bits_shift_amount, intptr_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(intptr_t) * CHAR_BIT )
		return true;

#		if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) == 202311L

	*result_pointer = value_original >> bits_shift_amount;

#		else

	if ( value_original >= 0 ) {
		*result_pointer = (intptr_t)(value_original >> bits_shift_amount);
	} else {
		uintptr_t value_unsigned = (uintptr_t) value_original;
		value_unsigned = (uintptr_t) ~( (uintptr_t)(~value_unsigned) >> bits_shift_amount);

		if ( value_unsigned > (uintptr_t) INTPTR_MAX ) {
			uintptr_t result_truncated = value_unsigned - ((uintptr_t) INTPTR_MAX + 1);
			*result_pointer = (intptr_t) result_truncated - INTPTR_MAX - 1;
		} else {
			*result_pointer = (intptr_t) value_unsigned;
		}
	}

#		endif /* __STDC_VERSION__ == 202311L */

	return false;
}
#	endif /* INTPTR_MAX */

#	ifdef UINTPTR_MAX
static inline bool sa_safe_shr_uintptr_t(
		uintptr_t value_original, size_t bits_shift_amount, uintptr_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		bits_shift_amount >= sizeof(uintptr_t) * CHAR_BIT )
		return true;

	*result_pointer = (uintptr_t)(value_original >> bits_shift_amount);
	return false;
}
#	endif /* UINTPTR_MAX */

#endif /* SAFE_SHIFT_H */
