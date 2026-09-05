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

/* Portable safe integer division.
 *
 * Every sa_ovf_div_<type> function divides two values of given type
 * without undefined behaviour, unlike the built-in '/' operator.
 *
 * Precondition:
 * result_pointer	- isn't NULL
 *
 * Arguments:
 * dividend			- dividend
 * divisor			- divisor
 * result_pointer	- pointer to store the quotient
 *
 * Returns:
 * true				- overflow occurred, zero divisor, result pointer is NULL
 * false			- division completed successfully
 */

#pragma once

#ifndef SAFE_DIVISION_H
#define SAFE_DIVISION_H

#	include "assert_m.h"/* assert_check_m */

#	include <stdint.h>	/* uint8_t	*/
#	include <stddef.h>	/* size_t	*/
#	include <stdbool.h>	/* bool		*/

#	if defined(UINT8_MAX)
static inline bool sa_ovf_div_uint8_t(
		uint8_t dividend, uint8_t divisor, uint8_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 )
		return true;

	*result_pointer = dividend / divisor;
	return false;
}
#	endif /* UINT8_MAX */

#	if defined(INT8_MIN)
static inline bool sa_ovf_div_int8_t(
		int8_t dividend, int8_t divisor, int8_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 || (divisor == -1 && dividend == INT8_MIN) )
		return true;

	*result_pointer = dividend / divisor;

	return false;
}
#	endif /* INT8_MIN */

#	if defined(UINT16_MAX)
static inline bool sa_ovf_div_uint16_t(
		uint16_t dividend, uint16_t divisor, uint16_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 )
		return true;

	*result_pointer = dividend / divisor;
	return false;
}
#	endif /* UINT16_MAX */

#	if defined(INT16_MIN)
static inline bool sa_ovf_div_int16_t(
		int16_t dividend, int16_t divisor, int16_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 || (divisor == -1 && dividend == INT16_MIN) )
		return true;

	*result_pointer = dividend / divisor;

	return false;
}
#	endif /* INT16_MIN */

#	if defined(UINT32_MAX)
static inline bool sa_ovf_div_uint32_t(
		uint32_t dividend, uint32_t divisor, uint32_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 )
		return true;

	*result_pointer = dividend / divisor;
	return false;
}
#	endif /* UINT32_MAX */

#	if defined(INT32_MIN)
static inline bool sa_ovf_div_int32_t(
		int32_t dividend, int32_t divisor, int32_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 || (divisor == -1 && dividend == INT32_MIN) )
		return true;

	*result_pointer = dividend / divisor;

	return false;
}
#	endif /* INT32_MIN */

#	if defined(UINT64_MAX)
static inline bool sa_ovf_div_uint64_t(
		uint64_t dividend, uint64_t divisor, uint64_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 )
		return true;

	*result_pointer = dividend / divisor;
	return false;
}
#	endif /* UINT64_MAX */

#	if defined(INT64_MIN)
static inline bool sa_ovf_div_int64_t(
		int64_t dividend, int64_t divisor, int64_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 || (divisor == -1 && dividend == INT64_MIN) )
		return true;

	*result_pointer = dividend / divisor;

	return false;
}
#	endif /* INT64_MIN */

static inline bool sa_ovf_div_size_t(
		size_t dividend, size_t divisor, size_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 )
		return true;

	*result_pointer = dividend / divisor;
	return false;
}

static inline bool sa_ovf_div_ptrdiff_t(
		ptrdiff_t dividend, ptrdiff_t divisor, ptrdiff_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 || (divisor == -1 && dividend == PTRDIFF_MIN) )
		return true;

	*result_pointer = dividend / divisor;

	return false;
}

static inline bool sa_ovf_div_uintmax_t(
		uintmax_t dividend, uintmax_t divisor, uintmax_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 )
		return true;

	*result_pointer = dividend / divisor;
	return false;
}

static inline bool sa_ovf_div_intmax_t(
		intmax_t dividend, intmax_t divisor, intmax_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 || (divisor == -1 && dividend == INTMAX_MIN) )
		return true;

	*result_pointer = dividend / divisor;

	return false;
}

#	ifdef UINTPTR_MAX
static inline bool sa_ovf_div_uintptr_t(
		uintptr_t dividend, uintptr_t divisor, uintptr_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 )
		return true;

	*result_pointer = dividend / divisor;
	return false;
}
#	endif /* UINTPTR_MAX */

#	ifdef INTPTR_MIN
static inline bool sa_ovf_div_intptr_t(
		intptr_t dividend, intptr_t divisor, intptr_t * result_pointer
	)
{
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false ||
		divisor == 0 || (divisor == -1 && dividend == INTPTR_MIN) )
		return true;

	*result_pointer = dividend / divisor;

	return false;
}
#	endif /* INTPTR_MIN */

#endif /* SAFE_DIVISION_H */
