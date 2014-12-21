//
// Obj Viewer in OpenGL 2.1
// Anton Gerdelan
// 21 Dec 2014
//
#include "maths_funcs.hpp"
#include "obj_parser.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // https://github.com/nothings/stb/
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // https://github.com/nothings/stb/
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

//
// for parsing CL params
int my_argc;
char** my_argv;

//
// dimensions of the window drawing surface
int gl_width = 800;
int gl_height = 800;

// shaders to use
char vs_file_name[256];
char fs_file_name[256];

// obj to load
char obj_file_name[256];

// texture file
char texture_file_name[256];

// built-in anti-aliasing to smooth jagged diagonal edges of polygons
int msaa_samples = 16;
// NOTE: if too high grainy crap appears on polygon edges

//
// check CL params for string. if found return argc value
// returns 0 if not present
// i stole this code from DOOM
int check_param (const char* s) {
	int i;

	for (i = 1; i < my_argc; i++) {
		if (!strcasecmp (s, my_argv[i])) {
			return i;
		}
	}

	return 0;
}

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

//
// take screenshot with F11
bool screencapture () {
	unsigned char* buffer = (unsigned char*)malloc (gl_width * gl_height * 3);
	glReadPixels (0, 0, gl_width, gl_height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	char name[1024];
	long int t = time (NULL);
	sprintf (name, "screenshot_%ld.png", t);
	unsigned char* last_row = buffer + (gl_width * 3 * (gl_height - 1));
	if (!stbi_write_png (name, gl_width, gl_height, 3, last_row, -3 * gl_width)) {
		fprintf (stderr, "ERROR: could not write screenshot file %s\n", name);
	}
	free (buffer);
	return true;
}

int main (int argc, char** argv) {
	GLFWwindow* window = NULL;
	const GLubyte* renderer;
	const GLubyte* version;
	GLuint shader_programme, normals_sp;
	int M_loc, V_loc, P_loc;
	int normals_M_loc, normals_V_loc, normals_P_loc;
	GLuint vao;
	int point_count = 0;
	int param = 0;
	float a = 0.0f;
	float scalef = 1.0f;
	double prev;
	vec3 vtra = vec3 (0.0f, 0.0f, 0.0f);
	char win_title[256];
	bool normals_mode = false;
	bool npressed = false;
	
	my_argc = argc;
	my_argv = argv;
	strcpy (vs_file_name, "shaders/basic.vert");
	strcpy (fs_file_name, "shaders/basic.frag");
	
	param = check_param ("--help");
	if (param) {
		printf ("\nOpenGL .obj Viewer.\nAnton Gerdelan 21 Dec 2014 @capnramses\n\n");
		printf ("usage: ./viewer [-o FILE] [-t FILE] [-vs FILE] [-fs FILE]\n\n");
		printf ("--help\t\t\tthis text\n");
		printf ("-o FILE\t\t\t.obj to load\n");
		printf ("-sca FLOAT\t\tscale mesh uniformly by this factor\n");
		printf ("-tra FLOAT FLOAT FLOAT\ttranslate mesh by X Y Z\n");
		printf ("-tex FILE\t\timage to use as texture\n");
		printf ("-vs FILE\t\tvertex shader to use\n");
		printf ("-vs FILE\t\tfragment shader to use\n");
		printf ("\n");
		printf ("F11\t\t\tscreenshot\n");
		printf ("n\t\t\ttoggle normals visualisation\n");
		printf ("\n");
		return 0;
	}
	
	param = check_param ("-o");
	if (param && my_argc > param + 1) {
		strcpy (obj_file_name, argv[param + 1]);
	} else {
		strcpy (obj_file_name, "cube.obj");
	}
	
	param = check_param ("-sca");
	if (param && my_argc > param + 1) {
		scalef = atof (argv[param + 1]);
	}
	
	param = check_param ("-tra");
	if (param && my_argc > param + 3) {
		vtra.v[0] = atof (argv[param + 1]);
		vtra.v[1] = atof (argv[param + 2]);
		vtra.v[2] = atof (argv[param + 3]);
	}
	
	param = check_param ("-tex");
	if (param && my_argc > param + 1) {
		strcpy (texture_file_name, argv[param + 1]);
	} else {
		strcpy (texture_file_name, "textures/checkerboard.png");
	}

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

	glfwWindowHint (GLFW_SAMPLES, msaa_samples);

	sprintf (win_title, "obj viewer: %s", obj_file_name);
	window = glfwCreateWindow (gl_width, gl_height, win_title, NULL, NULL);
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
		GLuint points_vbo, texcoord_vbo, normals_vbo;

		assert (load_obj_file (obj_file_name, vp, vt, vn, point_count));
	
		glGenBuffers (1, &points_vbo);
		glBindBuffer (GL_ARRAY_BUFFER, points_vbo);
		// copy our points from the header file into our VBO on graphics hardware
		glBufferData (GL_ARRAY_BUFFER, sizeof (float) * 3 * point_count, vp,
			GL_STATIC_DRAW);
		glGenBuffers (1, &texcoord_vbo);
		glBindBuffer (GL_ARRAY_BUFFER, texcoord_vbo);
		glBufferData (GL_ARRAY_BUFFER, sizeof (float) * 2 * point_count, vt,
			GL_STATIC_DRAW);
		glGenBuffers (1, &normals_vbo);
		glBindBuffer (GL_ARRAY_BUFFER, normals_vbo);
		glBufferData (GL_ARRAY_BUFFER, sizeof (float) * 3 * point_count, vn,
			GL_STATIC_DRAW);
	
		glGenVertexArrays (1, &vao);
		glBindVertexArray (vao);
		glEnableVertexAttribArray (0);
		glBindBuffer (GL_ARRAY_BUFFER, points_vbo);
		glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray (1);
		glBindBuffer (GL_ARRAY_BUFFER, texcoord_vbo);
		glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray (2);
		glBindBuffer (GL_ARRAY_BUFFER, normals_vbo);
		glVertexAttribPointer (2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		
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
		glBindAttribLocation (shader_programme, 0, "vp");
		glBindAttribLocation (shader_programme, 1, "vt");
		glBindAttribLocation (shader_programme, 2, "vn");
		glLinkProgram (shader_programme);
		M_loc = glGetUniformLocation (shader_programme, "M");
		V_loc = glGetUniformLocation (shader_programme, "V");
		P_loc = glGetUniformLocation (shader_programme, "P");
	}
	{
		char* vertex_shader_str = NULL;
		char* fragment_shader_str = NULL;
		GLuint vs, fs;
		
		// load shader strings from text files
		assert (parse_file_into_str ("shaders/normals.vert", &vertex_shader_str));
		assert (parse_file_into_str ("shaders/normals.frag",
			&fragment_shader_str));
		vs = glCreateShader (GL_VERTEX_SHADER);
		fs = glCreateShader (GL_FRAGMENT_SHADER);
		glShaderSource (vs, 1, (const char**)&vertex_shader_str, NULL);
		glShaderSource (fs, 1, (const char**)&fragment_shader_str, NULL);
		// free memory
		free (vertex_shader_str);
		free (fragment_shader_str);
		glCompileShader (vs);
		glCompileShader (fs);
		normals_sp = glCreateProgram ();
		glAttachShader (normals_sp, fs);
		glAttachShader (normals_sp, vs);
		glBindAttribLocation (normals_sp, 0, "vp");
		glBindAttribLocation (normals_sp, 1, "vt");
		glBindAttribLocation (normals_sp, 2, "vn");
		glLinkProgram (normals_sp);
		normals_M_loc = glGetUniformLocation (normals_sp, "M");
		normals_V_loc = glGetUniformLocation (normals_sp, "V");
		normals_P_loc = glGetUniformLocation (normals_sp, "P");
	}
	
	//
	// Create some matrices
	// --------------------------------------------------------------------------
	mat4 M, V, P, S, T;
	vec3 cam_pos (0.0, 0.0, 5.0);
	vec3 targ_pos (0.0, 0.0, 0.0);
	vec3 up (0.0, 1.0, 0.0);
	
	T = translate (identity_mat4 (), vtra);
	S = scale (identity_mat4 (), vec3 (scalef, scalef, scalef));
	M = T * S;
	V = look_at (cam_pos, targ_pos, up);
	P = perspective (67.0f, (float)gl_width / (float)gl_height, 0.1, 1000.0);
	
	// send matrix values to shader immediately
	glUseProgram (shader_programme);
	glUniformMatrix4fv (M_loc, 1, GL_FALSE, M.m);
	glUniformMatrix4fv (V_loc, 1, GL_FALSE, V.m);
	glUniformMatrix4fv (P_loc, 1, GL_FALSE, P.m);
	glUseProgram (normals_sp);
	glUniformMatrix4fv (normals_M_loc, 1, GL_FALSE, M.m);
	glUniformMatrix4fv (normals_V_loc, 1, GL_FALSE, V.m);
	glUniformMatrix4fv (normals_P_loc, 1, GL_FALSE, P.m);
	
	//
	// Create texture
	// --------------------------------------------------------------------------
	{
		int x,y,n;
		unsigned char* data;
		GLuint tex;
		
		data = stbi_load (texture_file_name, &x, &y, &n, 4);
		if (!data) {
			fprintf (stderr, "ERROR: could not load image %s\n", texture_file_name);
			return 1;
		}
		printf ("loaded image with %ix%ipx and %i chans\n", x, y, n);
		
		// NPOT check
		if ((x & (x - 1)) != 0 || (y & (y - 1)) != 0) {
			fprintf (stderr, "WARNING: texture is not power-of-two dimensions %s\n",
				texture_file_name);
		}
	
		// FLIP UP-SIDE DIDDLY-DOWN
		// make upside-down copy for GL
		{
			unsigned char *imagePtr = &data[0];
			int halfTheHeightInPixels = y / 2;
			int heightInPixels = y;
	
			// Assuming RGBA for 4 components per pixel.
			int numColorComponents = 4;
			// Assuming each color component is an unsigned char.
			int widthInChars = x * numColorComponents;
			unsigned char *top = NULL;
			unsigned char *bottom = NULL;
			unsigned char temp = 0;
			for (int h = 0; h < halfTheHeightInPixels; h++) {
				top = imagePtr + h * widthInChars;
				bottom = imagePtr + (heightInPixels - h - 1) * widthInChars;
				for (int w = 0; w < widthInChars; w++) {
					// Swap the chars around.
					temp = *top;
					*top = *bottom;
					*bottom = temp;
					++top;
					++bottom;
				}
			}
		}
		
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
	}
	
	//
	// Start rendering
	// --------------------------------------------------------------------------
	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LESS);
	glClearColor (0.5, 0.5, 0.8, 1.0);

	a = 0.0f;
	prev = glfwGetTime ();
	bool f11pressed = false;
	while (!glfwWindowShouldClose (window)) {
		double curr, elapsed;
	
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport (0, 0, gl_width, gl_height);
	
		curr = glfwGetTime ();
		elapsed = curr - prev;
		prev = curr;

		a += sinf (elapsed * 50.0f);
		M = T * rotate_y_deg (S, a);

		if (normals_mode) {
			glUseProgram (normals_sp);
			glUniformMatrix4fv (normals_M_loc, 1, GL_FALSE, M.m);
		} else {
			glUseProgram (shader_programme);
			glUniformMatrix4fv (M_loc, 1, GL_FALSE, M.m);
		}
		glBindVertexArray (vao);
		glDrawArrays (GL_TRIANGLES, 0, point_count);

		glfwPollEvents ();
		glfwSwapBuffers (window);
		
		if (GLFW_PRESS == glfwGetKey (window, GLFW_KEY_N)) {
			if (!npressed) {
				npressed = true;
				normals_mode = !normals_mode;
			}
		} else {
			npressed = false;
		}
		
		if (GLFW_PRESS == glfwGetKey (window, GLFW_KEY_F11)) {
			if (!f11pressed) {
				f11pressed = true;
				screencapture ();
			}
		} else {
			f11pressed = false;
		}
		
		if (GLFW_PRESS == glfwGetKey (window, GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose (window, 1);
		}
	}

	return 0;
}
