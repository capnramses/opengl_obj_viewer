#version 120

attribute vec3 vp; // points
attribute vec2 vt; // tex coords
attribute vec3 vn; // normals

uniform mat4 M, V, P;

varying vec3 n;

void main () {
	n = vec3 (V * M * vec4 (vn, 0.0));
	gl_Position = P * V * M * vec4 (vp, 1.0);
}
