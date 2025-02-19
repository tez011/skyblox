#include <GL/gl3w.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

static void print_gl_log(GLuint object)
{
	GLint log_length = 0;
	if (glIsShader(object))
		glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
	else if (glIsProgram(object))
		glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
	else {
		fprintf(stderr, "Object %u is not a shader or a program.\n", object);
		return;
	}

	char *log = malloc(log_length);
	if (glIsShader(object))
		glGetShaderInfoLog(object, log_length, NULL, log);
	else if (glIsProgram(object))
		glGetProgramInfoLog(object, log_length, NULL, log);

	fprintf(stderr, "%s", log);
	free(log);
}

GLuint create_shader_stage(const char *shader_path, GLenum type)
{
	size_t shader_length;
	GLchar *s = read_physfs_file(shader_path, &shader_length);
	if (s == NULL) {
		fprintf(stderr, "Couldn't open shader file %s: ", shader_path);
		perror("");
		return 0;
	}

	GLuint res = glCreateShader(type);
	glShaderSource(res, 1, (const GLchar *const *)&s, (const GLint *)&shader_length);
	free(s);

	GLint compile_ok = GL_FALSE;
	glCompileShader(res);
	glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok == GL_FALSE) {
		fprintf(stderr, "Compilation of %s: ", shader_path);
		print_gl_log(res);
		glDeleteShader(res);
		return 0;
	}
	return res;
}

inline GLuint create_shader(const char *vertex_shader_path, const char *fragment_shader_path)
{
	return create_gs_shader(vertex_shader_path, NULL, fragment_shader_path, 0, 0, 0);
}

GLuint create_gs_shader(const char *vertex_shader_path, const char *geom_shader_path, const char *fragment_shader_path, GLint input,
			GLint output, GLint vertices)
{
	GLuint program = glCreateProgram(), shader;
	if (vertex_shader_path) {
		shader = create_shader_stage(vertex_shader_path, GL_VERTEX_SHADER);
		if (shader == 0)
			return 0;
		else
			glAttachShader(program, shader);
	}
	if (geom_shader_path) {
		shader = create_shader_stage(geom_shader_path, GL_GEOMETRY_SHADER);
		if (shader == 0)
			return 0;
		else {
			glAttachShader(program, shader);
			glProgramParameteri(program, GL_GEOMETRY_INPUT_TYPE, input);
			glProgramParameteri(program, GL_GEOMETRY_OUTPUT_TYPE, output);
			glProgramParameteri(program, GL_GEOMETRY_VERTICES_OUT, vertices);
		}
	}
	if (fragment_shader_path) {
		shader = create_shader_stage(fragment_shader_path, GL_FRAGMENT_SHADER);
		if (shader == 0)
			return 0;
		else
			glAttachShader(program, shader);
	}

	GLint link_ok = GL_FALSE;
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
	if (link_ok == GL_FALSE) {
		fprintf(stderr, "glLinkProgram: ");
		print_gl_log(program);
		glDeleteProgram(program);
		return 0;
	}

	return program;
}

static GLuint active_program = 0;
void use_shader(GLuint program)
{
	glUseProgram(program);
	active_program = program;
}

GLint get_shader_attrib(GLuint program, const char *name)
{
	if (program == 0)
		program = active_program;

	return glGetAttribLocation(program, name);
}

GLint get_shader_uniform(GLuint program, const char *name)
{
	if (program == 0)
		program = active_program;

	GLuint loc = glGetUniformLocation(program, name);
	if (loc == -1)
		fprintf(stderr, "Could not bind uniform %s\n", name);
	return loc;
}
