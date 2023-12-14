/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file raster_import.cpp Modification of maps from multiple raster data types. */

#include "stdafx.h"
#include "raster_import.h"
#include "tree_base.h"
#include "tree_map.h"
#include "clear_map.h"
#include "heightmap.h"
#include "core/random_func.hpp"

#include "table/strings.h"

#include "safeguards.h"

/* Lower and upper cutoff thresholds for unique values in raster files.
 * Necessary for the rare case where an indexed raster with limited palette
 * may use values slightly further away from the lower and upper extremes. */
static const byte LOWER_CUTOFF = 0x0f;
static const byte UPPER_CUTOFF = 0xf0;

#define DEF_TILE_RASTER_FN(function) void function(byte r, byte g, byte b, TileIndex tile)

/**
 * A callback function type for performing an operation on a tile based on given raster data.
 *
 * @param r	 Red channel of raster data.
 * @param g  Green channel of raster data.
 * @param b  Blue channel of raster data.
 * @param tile Tile to perform the operation on.
 */
typedef void TileRasterCallback(byte r, byte g, byte b, TileIndex tile);

/**
 * Apply a per-tile function to the current map based on the pixel values of an RGB raster.
 * @param raster_width  Width of the raster.
 * @param raster_height  Height of the raster.
 * @param raster		 RGB-ordered byte array (24bpp) of raster data.
 * @tparam TileFunction  Pointer to the function to be run on each map tile, provided with RGB values.
 */
static void ApplyRasterToMap(uint raster_width, uint raster_height, byte *raster, TileRasterCallback proc)
{
	/* Defines the detail of the aspect ratio (to avoid doubles) */
	const uint num_div = 16384;
	/* Ensure multiplication with num_div does not cause overflows. */
	static_assert(num_div <= std::numeric_limits<uint>::max() / MAX_RASTER_SIDE_LENGTH_IN_PIXELS);

	uint map_width, map_height;
	uint map_row, map_col;
	uint map_row_pad = 0, map_col_pad = 0;
	uint raster_scale;
	uint raster_row, raster_col;
	uint r, g, b;
	TileIndex tile;

	/* Get map size and calculate scale and padding values */
	switch (_settings_game.game_creation.heightmap_rotation) {
		default: NOT_REACHED();
		case HM_COUNTER_CLOCKWISE:
			map_width = MapSizeX();
			map_height = MapSizeY();
			break;
		case HM_CLOCKWISE:
			map_width = MapSizeY();
			map_height = MapSizeX();
			break;
	}
	  
	if ((raster_width * num_div) / raster_height > ((map_width * num_div) / map_height)) {
	/* Image is wider than map - center vertically */
		raster_scale = (map_width * num_div) / raster_width;
		map_row_pad = (1 + map_height - ((raster_height * raster_scale) / num_div)) / 2;
	} else {
		/* Image is taller than map - center horizontally */
		raster_scale = (map_height * num_div) / raster_height;
		map_col_pad = (1 + map_width - ((raster_width * raster_scale) / num_div)) / 2;
	}

	/* Apply the TileHandler function to all valid map tiles */
	for (map_row = map_row_pad; map_row < (map_height - map_row_pad); map_row++) {
		for (map_col = map_col_pad; map_col < (map_width - map_col_pad); map_col++) {
			switch (_settings_game.game_creation.heightmap_rotation) {
				default: NOT_REACHED();
				case HM_COUNTER_CLOCKWISE: tile = TileXY(map_col, map_row); break;
				case HM_CLOCKWISE:         tile = TileXY(map_row, map_col); break;
			}

			/* Use nearest neighbour resizing to scale map data. */
			raster_row = (((map_row - map_row_pad) * num_div) / raster_scale);
			switch (_settings_game.game_creation.heightmap_rotation) {
				default: NOT_REACHED();
				case HM_COUNTER_CLOCKWISE:
					raster_col = (((map_width - 1 - map_col - map_col_pad) * num_div) / raster_scale);
					break;
				case HM_CLOCKWISE:
					raster_col = (((map_col - map_col_pad) * num_div) / raster_scale);
					break;
			}

			assert(raster_row < raster_height);
			assert(raster_col < raster_width);

			r = raster[(raster_row * raster_width + raster_col) * 3];
			g = raster[(raster_row * raster_width + raster_col) * 3 + 1];
			b = raster[(raster_row * raster_width + raster_col) * 3 + 2];

			if (IsInnerTile(tile)) {
				proc(r, g, b, tile);
			}
		}
	}
}

static void ReplaceGround(TileIndex tile, ClearGround ground, uint density = 3)
{
	TileType current = GetTileType(tile);

	if (current == MP_CLEAR) {
		SetClearGroundDensity(tile, ground, density);
	} else if (current == MP_TREES) {
		switch (ground) {
			case CLEAR_GRASS:
				SetTreeGroundDensity(tile, TREE_GROUND_GRASS, density);
				return;
			case CLEAR_ROUGH:
				SetTreeGroundDensity(tile, TREE_GROUND_ROUGH, density);
				return;
			case CLEAR_ROCKS:
			case CLEAR_FIELDS:
				MakeClear(tile, ground, density);
				return;
			case CLEAR_SNOW:
			case CLEAR_DESERT:
				SetTreeGroundDensity(tile, TREE_GROUND_SNOW_DESERT, density);
				return;
			default: NOT_REACHED();
		}
	}
}

/**
 * Samples a quantized gradient using random dithering and returns the level at the sample point.
 * @note x==start|   sample->|   |end
 * @note         |░░░|▒▒▒|▓▓▓|███|
 * @note y==     0   1   2  *3*  4==max_level
 * @param sample The x-value along the gradient at which to sample.
 * @param levels The number of quantized values that the gradient is.
 * @param start The x-value on the gradient where y==0.
 * @param end The x-value on the gradient where y==max_level.
 * @return The y-value of the gradient where x==sample, plus scaled randomised jitter.
 */
static int SampleQuantizedGradient(int sample, int max_level, int start, int end)
{
	uint rand;
	int x_delta, x;
	int y_jitter, y_delta, y;

	assert(max_level > 0);
	assert(start != end);

	/* Calculate the distance in x between quantized values */
	x_delta = (end - start) / max_level;

	/* Correct if start/end are reversed */
	if (start > end) std::swap(start, end);

	x = std::max(sample - start, 0);

	/* Use a random range with a buffer zone on either side, so that there can
	 * be fixed values in the gradient with no jitter. */
	rand = RandomRange(std::abs(x_delta - 2)) + 1;

	/* Use random to determine whether to jitter to the next level */
	y_jitter = (x % x_delta > rand) ? 1 : 0;

	/* Calculate the distance in y-levels from start to the current quantized value */
	y_delta = x / std::abs(x_delta);

	/* Apply jitter and clamp result to account for the case where sample is outside of start/end bounds */
	y = std::min(std::max(y_delta + y_jitter, 0), max_level);

	/* Invert results if start/end were originally reversed */
	if (x_delta < 0) y = max_level - y;
	
	return y;
}

/**
 * Modifies the basic terrain of a map tile based on the classification of given RGB values.
 * @note Terrain classification:
 *   Red:	 Grass->dirt tile density & probability
 *   Green:  Rough tile probability
 *   Blue:   Rock tile probability
 * @see TileRasterCallback()
 */
DEF_TILE_RASTER_FN(ApplyTerrain)
{
	uint density;
	TileType current = GetTileType(tile);

	if (current != MP_CLEAR && current != MP_TREES) return;

	/* Classify red channel values to grass->dirt density */
	if (r >= 0x10) {
		density = SampleQuantizedGradient(r, 3, UPPER_CUTOFF, LOWER_CUTOFF);
		if (density < 3) ReplaceGround(tile, CLEAR_GRASS, density);
	}

	/* Classify green channel values to rough tiles */
	if (g >= 0x10) {
		if (SampleQuantizedGradient(g, 1, LOWER_CUTOFF, UPPER_CUTOFF)) {
			ReplaceGround(tile, CLEAR_ROUGH);
		}
	}

	/* Classify blue channel values to rock tiles */
	if (b >= 0x10) {
		if (SampleQuantizedGradient(b, 1, LOWER_CUTOFF, UPPER_CUTOFF)) {
			ReplaceGround(tile, CLEAR_ROCKS);
		}
	}
}

/**
 * Converts a map tile to farm field based on the classification of given RGB values.
 * @note Terrain classification:
 *   Red   : Field type
 *   Green : Field probability
 *   Blue  : 
 // MAP TODO: implement IndustryID import
 * @see TileRasterCallback()
 */
DEF_TILE_RASTER_FN(ApplyFields)
{
	/* Determine field type by red channel */
	if (r < LOWER_CUTOFF) return;

	/* Compress pixel values so that each step of 16 in value represents a
	 * different field type. Wrap values larger than 9 */
	uint field = ((r >> 4) - 1) % 9;

	/* Determine field density by green channel */
	if (SampleQuantizedGradient(g, 1, LOWER_CUTOFF, UPPER_CUTOFF)) {
		MakeField(tile, field, INVALID_INDUSTRY);
	}
}

/**
 * Converts a map tile to water based on the classification of given RGB values.
 * @note Water classification:
 *   Red   : Create canal
 *   Green : Create river
 *   Blue  : Create sea
 * @see TileRasterCallback()
 */
DEF_TILE_RASTER_FN(ApplyWater)
{
	Slope slope;
	Owner owner;

	/* Attempt to make canal */
	if (r >= UPPER_CUTOFF) {
		slope = GetTileSlope(tile);
		if (slope == SLOPE_FLAT) {
			owner = GetTileOwner(tile);
			if (owner != OWNER_WATER) {
				MakeCanal(tile, owner, Random());
			} else {
				MakeCanal(tile, OWNER_NONE, Random());
			}
		/* Handle edge case where canal cannot be placed, but river can */
		} else if (g >= UPPER_CUTOFF && IsHalftileSlope(slope)) {
			MakeRiver(tile, Random());
		}
	/* Attempt to make river */
	} else if (g >= UPPER_CUTOFF) {
		slope = GetTileSlope(tile);
		if (slope == SLOPE_FLAT || IsHalftileSlope(slope)) {
			MakeRiver(tile, Random());
		}
	/* Attempt to make sea */
	} else if (b >= UPPER_CUTOFF) {
		if (IsTileFlat(tile) && TileHeight(tile) == 0) {
			MakeSea(tile);
		}
	}
}

static TreeType TreeTypeLookup(byte val)
{
	/* Compress pixel values so that each step of 16 in value represents a
	 * different tree type.
	 * Offset by 1 to allow for null tree when val < 0x10.
	 * Wrap values larger than TREE_COUNT_* */

	if (val < 0x10) return TREE_INVALID;

	switch (_settings_game.game_creation.landscape) {
		case LT_TEMPERATE:
			return (TreeType)(TREE_TEMPERATE + ((val >> 4) - 1) % TREE_COUNT_TEMPERATE);
		case LT_ARCTIC:
			return (TreeType)(TREE_SUB_ARCTIC + ((val >> 4) - 1) % TREE_COUNT_SUB_ARCTIC);
		case LT_TROPIC:
			return (TreeType)(TREE_RAINFOREST + ((val >> 4) - 1) % TREE_COUNT_SUB_TROPICAL);
		case LT_TOYLAND:
			return (TreeType)(TREE_TOYLAND + ((val >> 4) - 1) % TREE_COUNT_TOYLAND);
		default: NOT_REACHED();
	}
}

/**
 * Plants trees on a map tile based on the classification of given RGB values.
 * @note Tree classification:
 *   Red   : Tree growth
 *   Green : Tree density & probability
 *   Blue  : Tree type
 * @see TileRasterCallback()
 */
DEF_TILE_RASTER_FN(ApplyTrees)
{
	uint growth;
	int density;
	TreeType tree;

	/* Determine tree growth by red channel */
	if (r < 0x10) {
		/* Default to adult stage*/
		growth = 3;
	} else {
		growth = SampleQuantizedGradient(r, 6, LOWER_CUTOFF, UPPER_CUTOFF);
	}

	/* Determine tree density by green channel */
	/* Tree density between 0-3; scale g values in the range 0-4,
	/* subtract and discard negative values to create sparsity */
	density = SampleQuantizedGradient(g, 3+1, LOWER_CUTOFF, UPPER_CUTOFF) - 1;
	if (density < 0) return;

	/* Determine tree type by blue channel */
	if (b < 0x10) {
		/* Use random tree type */
		tree = GetRandomTreeType(tile, GB(Random(), 24, 8));
	} else {
		/* Use tree provided by lookup table */
		tree = TreeTypeLookup(b);
	}

	if (tree == TREE_INVALID) return;
	if (CanPlantTreesOnTile(tile, true)) {
		PlantTreesOnTile(tile, tree, density, growth);
	}
}

/**
 * Converts a map tile to snow based on the classification of given RGB values.
 * @note Snow classification:
 *   Red   : 
 *   Green : 
 *   Blue  : Snow density & probability
 * @see TileRasterCallback()
 */
DEF_TILE_RASTER_FN(ApplySnow)
{
	int density;
	TileType type;

	density = SampleQuantizedGradient(b, 3+1, LOWER_CUTOFF, UPPER_CUTOFF) - 1;
	if (density < 0) return;

	type = GetTileType(tile);

	if (type == MP_CLEAR) {
		if (IsSnowTile(tile)) {
			SetClearGroundDensity(tile, CLEAR_SNOW, density);
		} else {
			MakeSnow(tile, density);
		}
	} else if (type == MP_TREES) {
		switch (GetTreeGround(tile)) {
			case TREE_GROUND_GRASS:
			case TREE_GROUND_SNOW_DESERT:
				SetTreeGroundDensity(tile, TREE_GROUND_SNOW_DESERT, density);
				break;
			case TREE_GROUND_ROUGH:
			case TREE_GROUND_ROUGH_SNOW:
				SetTreeGroundDensity(tile, TREE_GROUND_ROUGH_SNOW, density);
				break;
		}
	}
}

/**
 * Converts a map tile to desert based on the classification of given RGB values.
 * @note Tree classification:
 *   Red   : Desert tile density & probability
 *   Green : Desert zone
 *   Blue  : 
 * @see TileRasterCallback()
 */
DEF_TILE_RASTER_FN(ApplyDesert)
{
	/* Density of desert can only be 1 or 3 */
	int density = (SampleQuantizedGradient(r, 1+1, LOWER_CUTOFF, UPPER_CUTOFF) * 2) - 1;
	if (density < 0) return;

	ReplaceGround(tile, CLEAR_DESERT, density);

	if (g > UPPER_CUTOFF) {
		SetTropicZone(tile, TROPICZONE_DESERT);
	}
}

/**
 * Sets tropic zone on a map tile based on the classification of given RGB values.
 * @note Zone classification:
 *   Red   : Desert zone
 *   Green : Rainforest zone
 *   Blue  : Normal zone
 * @see TileRasterCallback()
 */
DEF_TILE_RASTER_FN(ApplyTropics)
{
	if (r > UPPER_CUTOFF) {
		SetTropicZone(tile, TROPICZONE_DESERT);
	} else if (g > UPPER_CUTOFF) {
		SetTropicZone(tile, TROPICZONE_RAINFOREST);
	} else if (b > UPPER_CUTOFF) {
		SetTropicZone(tile, TROPICZONE_NORMAL);
	}
}

void LoadRaster(DetailedFileType dft, RasterDataType rdt, const char *filename, Subdirectory subdir)
{
	uint x, y;
	byte *raster = nullptr;

	if (!ReadRasterFile(dft, filename, subdir, &x, &y, &raster)) {
		free(raster);
		return;
	}

	switch (rdt) {
		case RDT_TERRAIN:
			ApplyRasterToMap(x, y, raster, ApplyTerrain);
			break;
		case RDT_FIELDS:
			ApplyRasterToMap(x, y, raster, ApplyFields);
			break;
		case RDT_WATER:
			ApplyRasterToMap(x, y, raster, ApplyWater);
			break;
		case RDT_TREES:
			ApplyRasterToMap(x, y, raster, ApplyTrees);
			break;
		case RDT_SNOW:
			ApplyRasterToMap(x, y, raster, ApplySnow);
			break;
		case RDT_DESERT:
			ApplyRasterToMap(x, y, raster, ApplyDesert);
			break;
		case RDT_TROPICS:
			ApplyRasterToMap(x, y, raster, ApplyTropics);
			break;
		default: NOT_REACHED();
	}

	free(raster);

	MarkWholeScreenDirty();
}
