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

/* Portable overflow-checked integer absolute value of a number.
 *
 * Every sa_ovf_abs_<type> functions computes the modulus of a given number
 * without undefined behaviour, unlike the standard `abs` function.
 *
 * Precondition:
 * result_pointer	- isn't NULL
 *
 * Arguments:
 * original_value	- value to take the modulus of
 * result_pointer	- pointer to store the modulus
 *
 * Returns:
 * true				- overflow occurred, result pointer is NULL
 * false			- absolute value computed successfully
 */

#pragma once

#ifndef SAFE_ABSOLUTE_H
#define SAFE_ABSOLUTE_H

#	include "assert_m.h"	/* assert_check_m	*/
#	include "safe_negate.h"	/* sa_ovf_neg_int8_t*/

#	include <stdint.h>		/* uint8_t			*/
#	include <stddef.h>		/* size_t			*/
#	include <stdbool.h>		/* bool				*/

#	if defined(INT8_MIN)
static inline bool sa_ovf_abs_int8_t( int8_t original_value, int8_t * result_pointer ) {
	if (assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( original_value >= 0 ) {
		*result_pointer = original_value;
		return false;
	}
	return sa_ovf_neg_int8_t( original_value, result_pointer );
}
#	endif /* INT8_MIN */

#	if defined(INT16_MIN)
static inline bool sa_ovf_abs_int16_t( int16_t original_value, int16_t * result_pointer ) {
	if (assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( original_value >= 0 ) {
		*result_pointer = original_value;
		return false;
	}
	return sa_ovf_neg_int16_t( original_value, result_pointer );
}
#	endif /* INT16_MIN */

#	if defined(INT32_MIN)
static inline bool sa_ovf_abs_int32_t( int32_t original_value, int32_t * result_pointer ) {
	if (assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( original_value >= 0 ) {
		*result_pointer = original_value;
		return false;
	}
	return sa_ovf_neg_int32_t( original_value, result_pointer );
}
#	endif /* INT32_MIN */

#	if defined(INT64_MIN)
static inline bool sa_ovf_abs_int64_t( int64_t original_value, int64_t * result_pointer ) {
	if (assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( original_value >= 0 ) {
		*result_pointer = original_value;
		return false;
	}
	return sa_ovf_neg_int64_t( original_value, result_pointer );
}
#	endif /* INT64_MIN */

static inline bool sa_ovf_abs_ptrdiff_t( ptrdiff_t original_value, ptrdiff_t * result_pointer ) {
	if (assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( original_value >= 0 ) {
		*result_pointer = original_value;
		return false;
	}
	return sa_ovf_neg_ptrdiff_t( original_value, result_pointer );
}

static inline bool sa_ovf_abs_intmax_t( intmax_t original_value, intmax_t * result_pointer ) {
	if (assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( original_value >= 0 ) {
		*result_pointer = original_value;
		return false;
	}
	return sa_ovf_neg_intmax_t( original_value, result_pointer );
}

#	ifdef INTPTR_MIN
static inline bool sa_ovf_abs_intptr_t( intptr_t original_value, intptr_t * result_pointer ) {
	if (assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

	if ( original_value >= 0 ) {
		*result_pointer = original_value;
		return false;
	}
	return sa_ovf_neg_intptr_t( original_value, result_pointer );
}
#	endif /* INTPTR_MIN */

#endif /* SAFE_ABSOLUTE_H */
