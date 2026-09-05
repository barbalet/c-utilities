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

#include "create_shader_program.h"

#include "write_out_error_message.h"
#include "gl_wrappers.h"
#include "assert_m.h"

#include <stdio.h>
#include <stdbool.h>

#include <GL/glew.h>
#include <GL/gl.h>

static GLuint csp_compile_shaders(
	GLenum type, const char * const * shader_sources_pointers, GLsizei sources_amount
);
static GLuint csp_link_shader_program(
	GLuint vertex_shader, GLuint fragment_shader, const char * caller_function_name_pointer
);

GLuint csp_create_shader_program (
		const char * restrict vertex_shader_source_pointer,
		const char * restrict fragment_shader_source_pointer
	)
{
	GLuint vertex_shader = 0, fragment_shader = 0;

	if( assert_check_m(
			vertex_shader_source_pointer != NULL && fragment_shader_source_pointer != NULL,
			"No shader sources found"
		) == false )
	{
		woem_push( "(csp_create_shader_program) invalid arguments" );
		return 0;
	}
	vertex_shader = csp_compile_shaders(
		GL_VERTEX_SHADER, (const char *[]){vertex_shader_source_pointer}, 1
	);
	if ( vertex_shader == 0 ) return 0;
	fragment_shader = csp_compile_shaders(
		GL_FRAGMENT_SHADER, (const char *[]){fragment_shader_source_pointer}, 1
	);
	if ( fragment_shader == 0 ) {
		(void) glDeleteShader_wrapped( vertex_shader );
		return 0;
	}

	return csp_link_shader_program(
		vertex_shader, fragment_shader, "csp_create_shader_program"
	);
}

GLuint csp_create_shader_program_many_sources (
		const char * const * restrict vertex_shader_sources_pointers,
		GLsizei vertex_shader_source_amount,
		const char * const * restrict fragment_shader_sources_pointers,
		GLsizei fragment_shader_source_amount
	)
{
	GLuint vertex_shader = 0, fragment_shader = 0;

	if( assert_check_m(
			vertex_shader_source_amount > 0 && fragment_shader_source_amount > 0,
			"Wrong shaders amount"
		) == false ||
		assert_check_m(
			vertex_shader_sources_pointers != NULL && fragment_shader_sources_pointers != NULL,
			"No shader sources found"
		) == false)
	{
		woem_push( "(csp_create_shader_program_many_sources) invalid arguments" );
		return 0;
	}

	vertex_shader = csp_compile_shaders(
		GL_VERTEX_SHADER, vertex_shader_sources_pointers, vertex_shader_source_amount
	);
	if ( vertex_shader == 0 ) return 0;
	fragment_shader = csp_compile_shaders(
		GL_FRAGMENT_SHADER, fragment_shader_sources_pointers, fragment_shader_source_amount
	);
	if ( fragment_shader == 0 ) {
		(void) glDeleteShader_wrapped( vertex_shader );
		return 0;
	}

	return csp_link_shader_program(
		vertex_shader, fragment_shader, "csp_create_shader_program_many_sources"
	);
}

/* Function:
 * link a shader into a shader program
 *
 * Parameters:
 * vertex_shader				- vertex shader source code string
 * fragment_shader				- fragment shader source code string
 * caller_function_name_pointer	- name of the calling function for error messages
 *
 * Returns:
 * non-zero						- shader program created
 * zero							- failed to create, failed to delete (DEBUG)
 */
static GLuint csp_link_shader_program(
		GLuint vertex_shader, GLuint fragment_shader, const char * caller_function_name_pointer
	)
{
	assert_m( vertex_shader					!= 0,	"No vertex shader found"		);
	assert_m( fragment_shader				!= 0,	"No fragment shader found"		);
	assert_m( caller_function_name_pointer	!= NULL,"No caller function name found"	);
	GLuint	shader_program	= 0;
	GLint	success			= 0;
	if ( glCreateProgram_wrapped( &shader_program ) == false )
		goto cleanup;

	if ( shader_program == 0 ) {
		woem_push( "(%s) shader program failed to create", caller_function_name_pointer );
		goto cleanup;
	}
	if ( glAttachShader_wrapped( shader_program, vertex_shader ) == false )
		goto cleanup;
	if ( glAttachShader_wrapped( shader_program, fragment_shader ) == false )
		goto cleanup;
	if ( glLinkProgram_wrapped( shader_program ) == false )
		goto cleanup;

	glGetProgramiv( shader_program, GL_LINK_STATUS, &success );
	if ( success == 0 ) {
		char log[512];
		glGetProgramInfoLog( shader_program, sizeof(log), NULL, log );
		woem_push(
			"(%s) shader program linking failed:\n%s", caller_function_name_pointer, log
		);
		goto cleanup;
	}

	if ( glDeleteShader_wrapped( vertex_shader ) == false ) {
		vertex_shader = 0;
		goto cleanup;
	}
	if ( glDeleteShader_wrapped( fragment_shader ) == false ) {
		fragment_shader = 0;
		goto cleanup;
	}
	return shader_program;

cleanup:
	if ( fragment_shader!= 0 ) (void) glDeleteShader_wrapped(	fragment_shader	);
	if ( vertex_shader	!= 0 ) (void) glDeleteShader_wrapped(	vertex_shader	);
	if ( shader_program	!= 0 ) (void) glDeleteProgram_wrapped(	shader_program	);
	return 0;
}

/* Function:
 * compile a shader of given type from given source code strings
 *
 * Parameters:
 * type				- shader type
 * shader_sources	- array of source code strings
 * sources_amount	- number of source code strings
 *
 * Returns:
 * non-zero			- shader created
 * zero				- failed to create
 */
static GLuint csp_compile_shaders(
		GLenum type, const char * const * shader_sources_pointers, GLsizei sources_amount
	)
{
	assert_mf(
		type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER,
		"Invalid shader type (%u)", type
	);
	GLint success = 0;
	GLuint shader = 0;

	if( assert_check_m( shader_sources_pointers	!= NULL,"No shader sources found")	== false ||
		assert_check_m( sources_amount			> 0,	"Wrong sources amount"	)	== false )
	{
		woem_push( "(csp_compile_shaders) invalid arguments" );
		goto cleanup;
	}

	if ( glCreateShader_wrapped( type, &shader ) == false )
		goto cleanup;
	if ( shader == 0 ) {
		woem_push( "(csp_compile_shaders) shader object creation failed (%d)", type );
		goto cleanup;
	}

	if (glShaderSource_wrapped( shader, sources_amount, shader_sources_pointers, NULL) == false )
		goto cleanup;
	if ( glCompileShader_wrapped( shader ) == false )
		goto cleanup;

	glGetShaderiv( shader, GL_COMPILE_STATUS, &success );
	if ( success == 0 ) {
		char log[512];
		glGetShaderInfoLog( shader, sizeof(log), NULL, log );
		woem_push(
			"(csp_compile_shaders) shader object compilation failed (%d):\n%s", type, log
		);
		goto cleanup;
	}

	return shader;

cleanup:
	if ( shader != 0 ) (void) glDeleteShader_wrapped( shader );
	return 0;
}
