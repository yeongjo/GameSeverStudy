#pragma once

struct Color {
	unsigned char b, g, r;
};

struct Image {
	Color* pixel{};
	int width{}, height{};

	Image() = default;
	Image(Color* pixel, int width, int height) : pixel(pixel), width(width), height(height) {}
	~Image();

	void ReadBmp(const char* filename);

	Color GetPixel(int x, int y) const;
};
