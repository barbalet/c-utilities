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

/* Portable overflow-checked integer exponentiation.
 *
 * Every sa_ovf_pow_<type> function raises the base to a non-negative
 * integer exponent without undefined behaviour.
 *
 * Precondition:
 * result_pointer	- isn't NULL
 *
 * Arguments:
 * base_value		- base value
 * exponent			- exponent
 * result_pointer	- pointer to store the result
 *
 * Returns:
 * true				- overflow occurred, result pointer is NULL
 * false			- exponentiation completed successfully
 */

#pragma once

#ifndef SAFE_POWER_H
#define SAFE_POWER_H

#	include "assert_m.h"			/* assert_check_m	*/
#	include "safe_multiplication.h"	/* sa_ovf_mul_int8_t*/

#	include <stdint.h>	/* uint8_t	*/
#	include <stddef.h>	/* size_t	*/
#	include <stdbool.h>	/* bool		*/

#	if defined(UINT8_MAX)
static inline bool sa_ovf_pow_uint8_t(
		uint8_t base_value, size_t exponent, uint8_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	uint8_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_uint8_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_uint8_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* UINT8_MAX */

#	if defined(INT8_MAX)
static inline bool sa_ovf_pow_int8_t(
		int8_t base_value, size_t exponent, int8_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	int8_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_int8_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_int8_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* INT8_MAX */

#	if defined(UINT16_MAX)
static inline bool sa_ovf_pow_uint16_t(
		uint16_t base_value, size_t exponent, uint16_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	uint16_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_uint16_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_uint16_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* UINT16_MAX */

#	if defined(INT16_MAX)
static inline bool sa_ovf_pow_int16_t(
		int16_t base_value, size_t exponent, int16_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	int16_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_int16_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_int16_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* INT16_MAX */

#	if defined(UINT32_MAX)
static inline bool sa_ovf_pow_uint32_t(
		uint32_t base_value, size_t exponent, uint32_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	uint32_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_uint32_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_uint32_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* UINT32_MAX */

#	if defined(INT32_MAX)
static inline bool sa_ovf_pow_int32_t(
		int32_t base_value, size_t exponent, int32_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	int32_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_int32_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_int32_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* INT32_MAX */

#	if defined(UINT64_MAX)
static inline bool sa_ovf_pow_uint64_t(
		uint64_t base_value, size_t exponent, uint64_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	uint64_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_uint64_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_uint64_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* UINT64_MAX */

#	if defined(INT64_MAX)
static inline bool sa_ovf_pow_int64_t(
		int64_t base_value, size_t exponent, int64_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	int64_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_int64_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_int64_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* INT64_MAX */

static inline bool sa_ovf_pow_size_t(
		size_t base_value, size_t exponent, size_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	size_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_size_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_size_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}

static inline bool sa_ovf_pow_ptrdiff_t(
		ptrdiff_t base_value, size_t exponent, ptrdiff_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	ptrdiff_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_ptrdiff_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_ptrdiff_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}

static inline bool sa_ovf_pow_uintmax_t(
		uintmax_t base_value, size_t exponent, uintmax_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	uintmax_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_uintmax_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_uintmax_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}

static inline bool sa_ovf_pow_intmax_t(
		intmax_t base_value, size_t exponent, intmax_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	intmax_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_intmax_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_intmax_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}

#	ifdef UINTPTR_MAX
static inline bool sa_ovf_pow_uintptr_t(
		uintptr_t base_value, size_t exponent, uintptr_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	uintptr_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_uintptr_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_uintptr_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* UINTPTR_MAX */

#	ifdef INTPTR_MAX
static inline bool sa_ovf_pow_intptr_t(
		intptr_t base_value, size_t exponent, intptr_t * result_pointer
	)
{
	if ( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( exponent == 0 ) {
		*result_pointer = 1;
		return false;
	}

	intptr_t result_temporary = 1;
	while ( exponent > 0 ) {
		if ( (exponent & 1) != 0 ) {
			if ( sa_ovf_mul_intptr_t( result_temporary, base_value, &result_temporary) == true )
				return true;
		}
		exponent >>= 1;
		if ( exponent > 0 ) {
			if ( sa_ovf_mul_intptr_t( base_value, base_value, &base_value ) == true )
				return true;
		}
	}

	*result_pointer = result_temporary;
	return false;
}
#	endif /* INTPTR_MAX */

#endif /* SAFE_POWER_H */
