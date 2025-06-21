#pragma once

class stb_impl {
public:
	static unsigned char* LoadImageFromFile(const char* filename, int* width, int* height, int* channels);
	static void FreeImageData(unsigned char* data);
};