#include "stb_impl.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned char* stb_impl::LoadImageFromFile(const char* filename, int* width, int* height, int* channels) {
	return stbi_load(filename, width, height, channels, 0);
}

void stb_impl::FreeImageData(unsigned char* data) {
	stbi_image_free(data);
}