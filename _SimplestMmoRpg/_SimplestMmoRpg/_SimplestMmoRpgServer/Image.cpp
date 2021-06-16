#include "Image.h"
#include <cstdio>
#include <iostream>

Image::~Image() {
	delete[] pixel;
}

void Image::ReadBmp(const char* filename) {
	int i;
	FILE* f;
	if (0 != fopen_s(&f, filename, "rb")){
		std::cout << "bmp open error: " << filename << std::endl;
		return;
	}
	unsigned char info[54];

	// read the 54-byte header
	fread(info, sizeof(unsigned char), 54, f);

	// extract image height and width from header
	width = *(int*)&info[18];
	height = *(int*)&info[22];

	// allocate 3 bytes per pixel
	int size = width * height;
	pixel = new Color[size];
	//unsigned char* pixel = new unsigned char[size*3];

	// read the rest of the data at once
	//fread(pixel, sizeof(unsigned char), size *3, f);
	fread(pixel, sizeof(unsigned char), size * 3, f);
	fclose(f);

	//Now data should contain the (R, G, B) values of the pixels. The color of pixel (i, j) is stored at data[3 * (i * width + j)], data[3 * (i * width + j) + 1] and data[3 * (i * width + j) + 2].
}

Color Image::GetPixel(int x, int y) const {
	_ASSERT(0 <= x && x < width);
	_ASSERT(0 <= y && y < height);
	return pixel[(height - y - 1) * width + x];
}
