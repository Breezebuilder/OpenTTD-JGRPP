/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file raster_io.h Functions related to loading and writing raster image files. */

#ifndef RASTER_IO_H
#define RASTER_IO_H

#include "fileio_type.h"

enum RasterDataType {
	RDT_HEIGHT = 0,
	RDT_TERRAIN,
	RDT_FIELDS,
	RDT_WATER,
	RDT_TREES,
	RDT_SNOW,
	RDT_DESERT,
	RDT_TROPICS,
	RDT_INVALID = 0xFF
};

/**
 * Maximum number of pixels for one dimension of a raster image.
 * Do not allow images for which the longest side is twice the maximum number of
 * tiles along the longest side of the (tile) map.
 */
static const uint MAX_RASTER_SIDE_LENGTH_IN_PIXELS = 2 * (1 << 16);

/*
 * Maximum size in pixels of the raster image.
 */
static const uint MAX_RASTER_SIZE_PIXELS = 256 << 20; // ~256 million
/*
 * When loading a PNG or BMP the 24 bpp variant requires at least 4 bytes per pixel
 * of memory to load the data. Make sure the "reasonable" limit is well within the
 * maximum amount of memory allocatable on 32 bit platforms.
 */
static_assert(MAX_RASTER_SIZE_PIXELS < UINT32_MAX / 8);

/**
 * Check whether the loaded dimension of the raster image are considered valid enough
 * to attempt to load the image. In other words, the width and height are not beyond the
 * #MAX_RASTER_SIDE_LENGTH_IN_PIXELS limit and the total number of pixels does not
 * exceed #MAX_RASTER_SIZE_PIXELS. A width or height less than 1 are disallowed too.
 * @param width The width of the to be loaded height map.
 * @param height The height of the to be loaded height map.
 * @return True iff the dimensions are within the limits.
 */
static inline bool IsValidRasterDimension(size_t width, size_t height)
{
	return (uint64)width * height <= MAX_RASTER_SIZE_PIXELS &&
		width > 0 && width <= MAX_RASTER_SIDE_LENGTH_IN_PIXELS &&
		height > 0 && height <= MAX_RASTER_SIDE_LENGTH_IN_PIXELS;
}

bool ReadRasterFile(DetailedFileType dft, const char *filename, Subdirectory subdir, uint *x, uint *y, byte **raster);

#endif /* RASTER_IO_H */
