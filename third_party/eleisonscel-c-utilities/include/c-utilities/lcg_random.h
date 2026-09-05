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

#pragma once

#ifndef LCG_RANDOM_H
#define LCG_RANDOM_H

#  include <stdint.h> /* uint64_t */

/* Function:
 * set a seed for a 64-bit linear congruential generator
 *
 * Parameters:
 * seed - value to set
 */
void lcg_set_random64( uint64_t seed );
/* Function:
 * set a seed for a 32-bit linear congruential generator
 *
 * Parameters:
 * seed - value to set
 */
void lcg_set_random32( uint32_t seed );

/* Function:
 * generate a 32-bit linear congruential generator in range of [0; max - 1] 
 *
 * Parameters:
 * max		- exclusive upper bound to generate a number
 *
 * Returns:
 * non-zero	- next pseudo random value
 * zero		- incorrect value or one
 */
uint32_t lcg_rand32_max( uint32_t max );
/* Function:
 * generate a 32-bit linear congruential generator in range of [0; UINT32_MAX]
 *
 * Returns:
 * value - next pseudo random value
 */
uint32_t lcg_random32(void);

/* Function:
 * generate a 64-bit linear congruential generator in range of [0; max - 1] 
 *
 * Parameters:
 * max		- exclusive upper bound to generate a number
 *
 * Returns:
 * non-zero	- next pseudo random value
 * zero		- incorrect value or one
 */
uint64_t lcg_rand64_max( uint64_t max );
/* Function:
 * generate a 64-bit linear congruential generator in range of [0; UINT64_MAX]
 *
 * Returns:
 * value - next pseudo random value
 */
uint64_t lcg_random64(void);

#endif /* LCG_RANDOM_H */
