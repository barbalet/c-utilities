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

/* Portable overflow-checked integer negation.
 *
 * Every sa_ovf_neg_<type> functions negates a value of the given type
 * without undefined behaviour, unlike the built-in unary `-` operator.
 *
 * Precondition:
 * result_pointer	- isn't NULL
 *
 * Arguments:
 * original_value	- value to negate
 * result_pointer	- pointer to store the negated value
 *
 * Returns:
 * true				- overflow occurred, result pointer is NULL
 * false			- negation completed successfully
 */

#pragma once

#ifndef SAFE_NEGATE_H
#define SAFE_NEGATE_H

#	include "assert_m.h"/* assert_check_m */

#	include <stdint.h>	/* uint8_t	*/
#	include <stddef.h>	/* size_t	*/
#	include <stdbool.h>	/* bool		*/

#	ifndef __has_builtin
#		define __has_builtin(x) 0
#	endif

#	if defined(__STDC_VERSION__) && (__STDC_VERSION__ + 0L) >= 202311L &&\
		defined(__has_include) && __has_include(<stdckdint.h>)
#		define SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD 1
#		include <stdckdint.h>
#	else
#		define SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD 0
#	endif /* stdckdint.h */

#	if (__has_builtin(__builtin_sub_overflow) != 0)
#		define SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN 1
#	else
#		define SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN 0
#	endif

#	if defined(INT8_MIN)
static inline bool sa_ovf_neg_int8_t( int8_t original_value, int8_t * result_pointer ) {
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

#		if SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD == 1

	return ckd_sub( result_pointer, 0, original_value );

#		elif SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN == 1

	return __builtin_sub_overflow( 0, original_value, result_pointer );

#		else

	if ( original_value == INT8_MIN )
		return true;

	*result_pointer = -original_value;
	return false;

#		endif
}
#	endif /* INT8_MIN */

#	if defined(INT16_MIN)
static inline bool sa_ovf_neg_int16_t( int16_t original_value, int16_t * result_pointer ) {
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

#		if SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD == 1

	return ckd_sub( result_pointer, 0, original_value );

#		elif SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN == 1

	return __builtin_sub_overflow( 0, original_value, result_pointer );

#		else

	if ( original_value == INT16_MIN )
		return true;

	*result_pointer = -original_value;
	return false;

#		endif
}
#	endif /* INT16_MIN */

#	if defined(INT32_MIN)
static inline bool sa_ovf_neg_int32_t( int32_t original_value, int32_t * result_pointer ) {
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

#		if SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD == 1

	return ckd_sub( result_pointer, 0, original_value );

#		elif SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN == 1

	return __builtin_sub_overflow( 0, original_value, result_pointer );

#		else

	if ( original_value == INT32_MIN )
		return true;

	*result_pointer = -original_value;
	return false;

#		endif
}
#	endif /* INT32_MIN */

#	if defined(INT64_MIN)
static inline bool sa_ovf_neg_int64_t( int64_t original_value, int64_t * result_pointer ) {
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

#		if SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD == 1

	return ckd_sub( result_pointer, 0, original_value );

#		elif SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN == 1

	return __builtin_sub_overflow( 0, original_value, result_pointer );

#		else

	if ( original_value == INT64_MIN )
		return true;

	*result_pointer = -original_value;
	return false;

#		endif
}
#	endif /* INT64_MIN */

static inline bool sa_ovf_neg_ptrdiff_t( ptrdiff_t original_value, ptrdiff_t * result_pointer ) {
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

#	if SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD == 1

	return ckd_sub( result_pointer, 0, original_value );

#	elif SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN == 1

	return __builtin_sub_overflow( 0, original_value, result_pointer );

#	else

	if ( original_value == PTRDIFF_MIN )
		return true;

	*result_pointer = -original_value;
	return false;

#	endif
}

static inline bool sa_ovf_neg_intmax_t( intmax_t original_value, intmax_t * result_pointer ) {
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

#	if SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD == 1

	return ckd_sub( result_pointer, 0, original_value );

#	elif SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN == 1

	return __builtin_sub_overflow( 0, original_value, result_pointer );

#	else

	if ( original_value == INTMAX_MIN )
		return true;

	*result_pointer = -original_value;
	return false;

#	endif
}

#	ifdef INTPTR_MIN
static inline bool sa_ovf_neg_intptr_t( intptr_t original_value, intptr_t * result_pointer ) {
	if( assert_check_m( result_pointer != NULL, "No place to store result found" ) == false )
		return true;

#		if SA_SAFE_HAS_OVERFLOW_SUBTRACTION_STANDARD == 1

	return ckd_sub( result_pointer, 0, original_value );

#		elif SA_SAFE_HAS_OVERFLOW_SUBTRACTION_BUILTIN == 1

	return __builtin_sub_overflow( 0, original_value, result_pointer );

#		else

	if ( original_value == INTPTR_MIN )
		return true;

	*result_pointer = -original_value;
	return false;

#		endif
}
#	endif /* INTPTR_MIN */

#endif /* SAFE_NEGATE_H */
