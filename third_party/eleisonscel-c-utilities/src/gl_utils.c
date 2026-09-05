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

#include "gl_utils.h"

#include "write_out_error_message.h"
#include "assert_m.h"

#include <stdbool.h>

bool gl_errors_check(
		const char * restrict function_name_pointer
	)
{
	assert_m( function_name_pointer != NULL, "No function name specified" );

	GLenum gl_error_code = glGetError();
	if ( gl_error_code == GL_NO_ERROR ) return true;

	woem_push(
		"(%s): %s", (function_name_pointer != NULL) ?
			function_name_pointer : "gl_errors_check", gl_get_error_string( gl_error_code )
	);

	return false;
}

const char * gl_get_error_string( GLenum gl_error_code ) {
	switch( gl_error_code ) {
		case GL_NO_ERROR:
			return "(GL_NO_ERROR) No error has been recorded";
		case GL_INVALID_ENUM:
			return "(GL_INVALID_ENUM) An unacceptable value is specified for an enumerated argument";
		case GL_INVALID_VALUE:
			return "(GL_INVALID_VALUE) A numeric argument is out of range";
		case GL_INVALID_OPERATION:
			return
				"(GL_INVALID_OPERATION) The specified operation is not allowed in the current state";
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			return "(GL_INVALID_FRAMEBUFFER_OPERATION) The framebuffer object is not complete";
		case GL_OUT_OF_MEMORY:
			return "(GL_OUT_OF_MEMORY) There is not enough memory left to execute the command";
#ifdef GL_STACK_UNDERFLOW
		case GL_STACK_UNDERFLOW:
			return "(GL_STACK_UNDERFLOW) Operation would cause an internal stack to underflow";
#endif /* GL_STACK_UNDERFLOW */
#ifdef GL_STACK_OVERFLOW
		case GL_STACK_OVERFLOW:
			return "(GL_STACK_OVERFLOW) Operation would cause an internal stack to overflow";
#endif /* GL_STACK_OVERFLOW */
		default: return "(Unknown Error)";
	}
}
