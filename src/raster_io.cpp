/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file raster_io.cpp Loading and writing of raster image files. */

#include "stdafx.h"
#include "raster_io.h"
#include "core/alloc_func.hpp"
#include "fileio_func.h"
#include "error.h"
#include "bmp.h"

#include "table/strings.h"

#include "safeguards.h"

#ifdef WITH_PNG

#include <png.h>

/**
 * Reads PNG data with or without a palette to RGB byte arrays.
 * Each row is read to a byte array, where each pixel will have 3 corresponding
 * array entries for each of the values of the Red, Green and Blue channels.
 * Greyscale PNGs are expanded to RGB.
 */
static void ReadPNGRows(byte *raster, png_structp png_ptr, png_infop info_ptr)
{
	uint i, x, y;
	uint width, height;
	png_bytep *row_pointers = nullptr;
	png_color *palette = nullptr;
	bool has_palette = png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE;
	uint channels = png_get_channels(png_ptr, info_ptr);

	row_pointers = png_get_rows(png_ptr, info_ptr);
	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);

	if (has_palette) {
		/* Read indexed PNG (with color palette) */
		int palette_size;

		png_get_PLTE(png_ptr, info_ptr, &palette, &palette_size);

		for (x = 0; x < width; x++) {
			for (y = 0; y < height; y++) {
				byte *pixel = &raster[(y * width + x) * 3];
				uint x_offset = x * channels;

				*pixel++ = palette[row_pointers[y][x_offset]].red;
				*pixel++ = palette[row_pointers[y][x_offset]].green;
				*pixel++ = palette[row_pointers[y][x_offset]].blue;
			}
		}
	} else {
		/* Read non-indexed PNG - no palette, 24bpp RGB or 8bpp Gray */
		for (x = 0; x < width; x++) {
			for (y = 0; y < height; y++) {
				byte *pixel = &raster[(y * width + x) * 3];
				uint x_offset = x * channels;

				if (channels == 3) {
					*pixel++ = row_pointers[y][x_offset + 0];
					*pixel++ = row_pointers[y][x_offset + 1];
					*pixel++ = row_pointers[y][x_offset + 2];
				} else {
					for (i = 0; i < 3; i++) {
						*pixel++ = row_pointers[y][x_offset];
					}
				}
			}
		}
	}

}

/**
 * Reads raster data and/or size of the image from a PNG file.
 * If raster == nullptr only the size of the PNG is read, otherwise the
 * RGB raster data is read and assigned to *raster.
 */
static bool ReadRasterPNG(FILE *fp, uint *width, uint *height, byte **raster)
{
	png_structp png_ptr = nullptr;
	png_infop info_ptr = nullptr;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (png_ptr == nullptr) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_MISC, WL_ERROR);
		fclose(fp);
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr || setjmp(png_jmpbuf(png_ptr))) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_MISC, WL_ERROR);
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return false;
	}

	png_init_io(png_ptr, fp);

	/* Allocate memory and read image, without alpha or 16-bit samples
	 * (result is either 8-bit indexed/grayscale or 24-bit RGB) */
	png_set_packing(png_ptr);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_PACKING | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16, nullptr);

	/* Maps of wrong colour-depth are not used.
	 * (this should have been taken care of by stripping alpha and 16-bit samples on load) */
	if ((png_get_channels(png_ptr, info_ptr) != 1) && (png_get_channels(png_ptr, info_ptr) != 3) && (png_get_bit_depth(png_ptr, info_ptr) != 8)) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_IMAGE_TYPE, WL_ERROR);
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return false;
	}

	uint raster_width = png_get_image_width(png_ptr, info_ptr);
	uint raster_height = png_get_image_height(png_ptr, info_ptr);

	if (!IsValidRasterDimension(raster_width, raster_height)) {
		ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_HEIGHTMAP_TOO_LARGE, WL_ERROR);
		fclose(fp);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return false;
	}

	if (raster != nullptr) {
		*raster = MallocT<byte>(static_cast<size_t>(raster_width) * raster_height * 3);
		ReadPNGRows(*raster, png_ptr, info_ptr);
	}

	*width = raster_width;
	*height = raster_height;

	fclose(fp);
	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	return true;
}

#endif // WITH_PNG

/**
 * Reads BMP data with or without a palette to RGB byte arrays.
 * Each row is read to a byte array, where each pixel will have 3 corresponding
 * array entries for each of the values of the Red, Green and Blue channels.
 * BMP ARGB is not supported.
 */
static void ReadBMPRows(byte *raster, BmpInfo *info, BmpData *data)
{
	uint x, y;

	if (data->palette != nullptr) {
		/* Read indexed BMP (with color palette) */
		for (y = 0; y < info->height; y++) {
			byte *pixel = &raster[y * info->width * 3];
			byte *bitmap = &data->bitmap[y * info->width];

			for (x = 0; x < info->width; x++) {
				*pixel++ = data->palette[*bitmap].r;
				*pixel++ = data->palette[*bitmap].g;
				*pixel++ = data->palette[*bitmap].b;
				bitmap++;
			}
		}
	} else if (info->bpp == 24) {
		/* Read non-indexed BMP (no palette, 24bpp RGB only) */
		for (y = 0; y < info->height; y++) {
			byte *pixel = &raster[y * info->width * 3];
			byte *bitmap = &data->bitmap[y * info->width * 3];

			for (x = 0; x < info->width * 3; x++) {
				*pixel++ = *bitmap;
				bitmap++;
			}
		}
	}
}

/**
 * Reads raster data and/or size of the image from a BMP file.
 * If raster == nullptr only the size of the BMP is read, otherwise the
 * RGB raster data is read and assigned to *raster.
 */
static bool ReadRasterBMP(FILE *fp, uint *width, uint *height, byte **raster)
{
	BmpInfo info;
	BmpData data;
	BmpBuffer buffer;

	/* Initialize BmpData */
	memset(&data, 0, sizeof(data));

	BmpInitializeBuffer(&buffer, fp);

	if (!BmpReadHeader(&buffer, &info, &data)) {
		ShowErrorMessage(STR_ERROR_BMPMAP, STR_ERROR_BMPMAP_IMAGE_TYPE, WL_ERROR);
		fclose(fp);
		BmpDestroyData(&data);
		return false;
	}

	if (!IsValidRasterDimension(info.width, info.height)) {
		ShowErrorMessage(STR_ERROR_BMPMAP, STR_ERROR_HEIGHTMAP_TOO_LARGE, WL_ERROR);
		fclose(fp);
		BmpDestroyData(&data);
		return false;
	}

	if (raster != nullptr) {
		if (!BmpReadBitmap(&buffer, &info, &data)) {
			ShowErrorMessage(STR_ERROR_BMPMAP, STR_ERROR_BMPMAP_IMAGE_TYPE, WL_ERROR);
			fclose(fp);
			BmpDestroyData(&data);
			return false;
		}

		*raster = MallocT<byte>(static_cast<size_t>(info.width) * info.height * 3);
		ReadBMPRows(*raster, &info, &data);
	}

	BmpDestroyData(&data);

	*width = info.width;
	*height = info.height;

	fclose(fp);
	return true;
}

/**
 * Reads RGB channels of a map raster with the correct file reader.
 * @param dft Type of image file.
 * @param filename Name of the file to load.
 * @param subdir Subdirectory to load the file from.
 * @param[out] x Length of the image.
 * @param[out] y Height of the image.
 * @param[in,out] map If not \c nullptr, destination to store the loaded block of image data.
 * @return Whether loading was successful.
 */
bool ReadRasterFile(DetailedFileType dft, const char *filename, Subdirectory subdir, uint *x, uint *y, byte **raster)
{
	FILE *file;
	file = FioFOpenFile(filename, "rb", subdir);

	switch (dft) {
		default:
			NOT_REACHED();

		case DFT_HEIGHTMAP_PNG:
			if (file == nullptr) {
				ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_FILE_NOT_FOUND, WL_ERROR);
				return false;
			}
			return ReadRasterPNG(file, x, y, raster);

		case DFT_HEIGHTMAP_BMP:
			if (file == nullptr) {
				ShowErrorMessage(STR_ERROR_PNGMAP, STR_ERROR_PNGMAP_FILE_NOT_FOUND, WL_ERROR);
				return false;
			}
			return ReadRasterBMP(file, x, y, raster);
	}
}
