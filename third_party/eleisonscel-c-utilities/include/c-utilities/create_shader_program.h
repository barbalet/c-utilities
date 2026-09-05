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

#ifndef CSP_CREATE_SHADER_PROGRAM_H
#define CSP_CREATE_SHADER_PROGRAM_H

#  include <GL/glew.h>  /* GLuint */

/* Function:
 * create a shader program from a single shader source string
 *
 * Parameters:
 * vertex_shader_source_pointer		- vertex shader source code string
 * fragment_shader_source_pointer	- fragment shader source code string
 *
 * Returns:
 * non-zero							- shader created
 * zero								- incorrect arguments, failed to create, failed to delete
 *										(DEBUG)
 */
GLuint csp_create_shader_program( const char * restrict vertex_shader_source_pointer, const char * restrict fragment_shader_source_pointer );
/* Function:
 * create a shader program from many shader source string
 *
 * Parameters:
 * vertex_shader_sources_pointers	- array of vertex shader source string
 * vertex_shader_source_amount		- number of vertex shader sources
 * fragment_shader_source_pointers	- array of fragment shader source string
 * fragment_shader_source_amount	- number of fragment shader sources
 *
 * Returns:
 * non-zero							- shader created
 * zero								- incorrect arguments, failed to create,
 *										failed to delete (DEBUG)
 */
GLuint csp_create_shader_program_many_sources( const char * const * restrict vertex_shader_sources_pointers, GLsizei vertex_shader_source_amount, const char * const * restrict fragment_shader_sources_pointers, GLsizei fragment_shader_source_amount );

#endif /* CSP_CREATE_SHADER_PROGRAM_H */
