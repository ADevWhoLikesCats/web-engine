#ifndef TEXTURE_H
#define TEXTURE_H

#include <GLES3/gl3.h>

/* Create a texture from an in-memory image (PNG/JPG), return GL handle.
   srgb=1 for basecolor/emissive, 0 for linear data (normal, MR, occlusion).
   Returns 0 on failure. */
GLuint texture_from_memory(const unsigned char* bytes, int len, int srgb);

/* Create a 1x1 solid-color texture, useful as a fallback. */
GLuint texture_solid(unsigned char r, unsigned char g, unsigned char b, unsigned char a, int srgb);

#endif
