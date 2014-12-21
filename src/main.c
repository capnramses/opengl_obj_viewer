//
// Obj Viewer in OpenGL 2.1
// Anton Gerdelan
// 21 Dec 2014
//
#include "maths_funcs.hpp"
#include "obj_parser.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

//
// dimensions of the window drawing surface
int gl_width = 800;
int gl_height = 800;

//
// shaders to use
char vs_file_name[256];
char fs_file_name[256];

//
// copy a shader from a plain text file into a character array
bool parse_file_into_str (const char* file_name, char** shader_str) {
	FILE* file;
	long sz;
	char line[2048];

	line[0] = '\0';
	
	file = fopen (file_name , "r");
	if (!file) {
		fprintf (stderr, "ERROR: opening file for reading: %s\n", file_name);
		return false;
	}
	
	// get file size and allocate memory for string
	assert (0 == fseek (file, 0, SEEK_END));
	sz = ftell (file);
	rewind (file);
	*shader_str = (char*)malloc (sz);
	*shader_str[0] = '\0';
	
	while (!feof (file)) {
		if (fgets (line, 2048, file)) {
			strcat (*shader_str, line);
		}
	}

	return true;
}

int main () {
	GLFWwindow* window = NULL;
	const GLubyte* renderer;
	const GLubyte* version;
	GLuint shader_programme;
	GLuint vao;
	int point_count = 0;
	float a = 0.0f;
	double prev;
	
	strcpy (vs_file_name, "shaders/basic.vert");
	strcpy (fs_file_name, "shaders/basic.frag");

	//
	// Start OpenGL using helper libraries
	// --------------------------------------------------------------------------
	if (!glfwInit ()) {
		fprintf (stderr, "ERROR: could not start GLFW3\n");
		return 1;
	} 

	/* change to 3.2 if on Apple OS X
	glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); */

	window = glfwCreateWindow (gl_width, gl_height, "Spinning Cube", NULL, NULL);
	if (!window) {
		fprintf (stderr, "ERROR: opening OS window\n");
		return 1;
	}
	glfwMakeContextCurrent (window);

	glewExperimental = GL_TRUE;
	glewInit ();

	renderer = glGetString (GL_RENDERER);
	version = glGetString (GL_VERSION);
	printf ("Renderer: %s\n", renderer);
	printf ("OpenGL version supported %s\n", version);

	//
	// Set up vertex buffers and vertex array object
	// --------------------------------------------------------------------------
	{
		GLfloat* vp = NULL; // array of vertex points
		GLfloat* vn = NULL; // array of vertex normals (we haven't used these yet)
		GLfloat* vt = NULL; // array of texture coordinates (or these)
		GLuint points_vbo, texcoord_vbo;

		assert (load_obj_file ("cube.obj", vp, vt, vn, point_count));
	
		glGenBuffers (1, &points_vbo);
		glBindBuffer (GL_ARRAY_BUFFER, points_vbo);
		// copy our points from the header file into our VBO on graphics hardware
		glBufferData (GL_ARRAY_BUFFER, sizeof (float) * 3 * point_count, vp,
			GL_STATIC_DRAW);
		// and grab the normals
		glGenBuffers (1, &texcoord_vbo);
		glBindBuffer (GL_ARRAY_BUFFER, texcoord_vbo);
		glBufferData (GL_ARRAY_BUFFER, sizeof (float) * 2 * point_count, vt,
			GL_STATIC_DRAW);
	
		glGenVertexArrays (1, &vao);
		glBindVertexArray (vao);
		glEnableVertexAttribArray (0);
		glBindBuffer (GL_ARRAY_BUFFER, points_vbo);
		glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray (1);
		glBindBuffer (GL_ARRAY_BUFFER, texcoord_vbo);
		glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		
		free (vp);
		free (vn);
		free (vt);
	}
	
	//
	// Load shaders from files
	// --------------------------------------------------------------------------
	{
		char* vertex_shader_str = NULL;
		char* fragment_shader_str = NULL;
		GLuint vs, fs;
		
		// load shader strings from text files
		assert (parse_file_into_str (vs_file_name, &vertex_shader_str));
		assert (parse_file_into_str (fs_file_name, &fragment_shader_str));
		vs = glCreateShader (GL_VERTEX_SHADER);
		fs = glCreateShader (GL_FRAGMENT_SHADER);
		glShaderSource (vs, 1, (const char**)&vertex_shader_str, NULL);
		glShaderSource (fs, 1, (const char**)&fragment_shader_str, NULL);
		// free memory
		free (vertex_shader_str);
		free (fragment_shader_str);
		glCompileShader (vs);
		glCompileShader (fs);
		shader_programme = glCreateProgram ();
		glAttachShader (shader_programme, fs);
		glAttachShader (shader_programme, vs);
		glLinkProgram (shader_programme);
	}
	
	//
	// Create some matrices
	// --------------------------------------------------------------------------
	mat4 M, V, P;
	vec3 cam_pos (0.0, 0.0, 5.0);
	vec3 targ_pos (0.0, 0.0, 0.0);
	vec3 up (0.0, 1.0, 0.0);
	int M_loc, V_loc, P_loc;
	
	M = identity_mat4 ();//scale (identity_mat4 (), vec3 (0.05, 0.05, 0.05));
	V = look_at (cam_pos, targ_pos, up);
	P = perspective (67.0f, (float)gl_width / (float)gl_height, 0.1, 1000.0);
	
	M_loc = glGetUniformLocation (shader_programme, "M");
	V_loc = glGetUniformLocation (shader_programme, "V");
	P_loc = glGetUniformLocation (shader_programme, "P");
	// send matrix values to shader immediately
	glUseProgram (shader_programme);
	glUniformMatrix4fv (M_loc, 1, GL_FALSE, M.m);
	glUniformMatrix4fv (V_loc, 1, GL_FALSE, V.m);
	glUniformMatrix4fv (P_loc, 1, GL_FALSE, P.m);
	
	/*int x,y,n;
	unsigned char *data = stbi_load ("sword.png", &x, &y, &n, 0);
	if (!data) {
		fprintf (stderr, "ERROR: could not load image\n");
		return 1;
	}
	printf ("loaded image with [%i,%i] res and %i chans\n", x, y, n);
	GLuint tex;
	glGenTextures (1, &tex);
	glActiveTexture (GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_2D, tex);
	glTexImage2D (
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		x,
		y,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		data
	);
	stbi_image_free(data);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	*/
	
	//
	// Start rendering
	// --------------------------------------------------------------------------
	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LESS);
	glClearColor (0.01, 0.01, 0.25, 1.0);

	a = 0.0f;
	prev = glfwGetTime ();
	while (!glfwWindowShouldClose (window)) {
		double curr, elapsed;
	
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport (0, 0, gl_width, gl_height);
	
		curr = glfwGetTime ();
		elapsed = curr - prev;
		prev = curr;
	
		glUseProgram (shader_programme);
		glBindVertexArray (vao);
	
		a += sinf (elapsed * 50.0f);
		M = rotate_y_deg (identity_mat4 (), a);
		glUniformMatrix4fv (M_loc, 1, GL_FALSE, M.m);
	
		glDrawArrays (GL_TRIANGLES, 0, point_count);

		glfwPollEvents ();
		glfwSwapBuffers (window);
	}

	return 0;
}
