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

#ifndef GL_UTILS_H
#define GL_UTILS_H

#	include <stdbool.h> /* bool */

#	include <GL/glew.h>	/* GLenum		*/
#	include <GL/gl.h>	/* glGetError	*/

/* Function:
 * clean OpenGL errors
 */
static inline void gl_errors_clean(void) {
	while( glGetError() != GL_NO_ERROR );
}

/* Function:
 * check if there was any OpenGL error
 *
 * function_name		- called function name
 *
 * Returns:
 * true					- no errors found
 * false				- an error found
 */
bool gl_errors_check( const char * restrict function_name_pointer );

/* Function:
 * get description of some OpenGL error code
 *
 * Parameters:
 * gl_get_error_code	- error code to handle
 *
 * Returns:
 * string				- error description
 */
const char * gl_get_error_string( GLenum gl_error_code );

#endif /* GL_UTILS_H */
