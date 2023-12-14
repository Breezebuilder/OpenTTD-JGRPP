/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tree_base.h Base for tree tiles. */

#include "tile_type.h"

#ifndef TREE_BASE_H
#define TREE_BASE_H

/**
 * List of tree types along all landscape types.
 *
 * This enumeration contains a list of the different tree types along
 * all landscape types. The values for the enumerations may be used for
 * offsets from the grfs files. These points to the start of
 * the tree list for a landscape. See the TREE_COUNT_* enumerations
 * for the amount of different trees for a specific landscape.
 */
enum TreeType {
	TREE_TEMPERATE = 0x00, ///< temperate tree
	TREE_SUB_ARCTIC = 0x0C, ///< tree on a sub_arctic landscape
	TREE_RAINFOREST = 0x14, ///< tree on the 'green part' on a sub-tropical map
	TREE_CACTUS = 0x1B, ///< a cactus for the 'desert part' on a sub-tropical map
	TREE_SUB_TROPICAL = 0x1C, ///< tree on a sub-tropical map, non-rainforest, non-desert
	TREE_TOYLAND = 0x20, ///< tree on a toyland map
	TREE_INVALID = 0xFF, ///< An invalid tree
};

/* Counts the number of tree types for each landscape.
 *
 * This list contains the counts of different tree types for each landscape. This list contains
 * 5 entries instead of 4 (as there are only 4 landscape types) as the sub tropic landscape
 * has two types of area, one for normal trees and one only for cacti.
 */
static const uint TREE_COUNT_TEMPERATE = TREE_SUB_ARCTIC - TREE_TEMPERATE;    ///< number of tree types on a temperate map.
static const uint TREE_COUNT_SUB_ARCTIC = TREE_RAINFOREST - TREE_SUB_ARCTIC;   ///< number of tree types on a sub arctic map.
static const uint TREE_COUNT_RAINFOREST = TREE_CACTUS - TREE_RAINFOREST;   ///< number of tree types for the 'rainforest part' of a sub-tropic map.
static const uint TREE_COUNT_SUB_TROPICAL = TREE_TOYLAND - TREE_SUB_TROPICAL; ///< number of tree types for the 'sub-tropic part' of a sub-tropic map.
static const uint TREE_COUNT_TOYLAND = 9;                                   ///< number of tree types on a toyland map.

/**
 * Enumeration for ground types of tiles with trees.
 *
 * This enumeration defines the ground types for tiles with trees on it.
 */
enum TreeGround {
	TREE_GROUND_GRASS = 0, ///< normal grass
	TREE_GROUND_ROUGH = 1, ///< some rough tile
	TREE_GROUND_SNOW_DESERT = 2, ///< a desert or snow tile, depend on landscape
	TREE_GROUND_SHORE = 3, ///< shore
	TREE_GROUND_ROUGH_SNOW = 4, ///< A snow tile that is rough underneath.
};

void PlaceTreesRandomly();
void RemoveAllTrees();
uint PlaceTreeGroupAroundTile(TileIndex tile, TreeType treetype, uint radius, uint count, bool set_zone);

bool CanPlantTreesOnTile(TileIndex tile, bool allow_desert);
void PlantTreesOnTile(TileIndex tile, TreeType treetype, uint count, uint growth);
TreeType GetRandomTreeType(TileIndex tile, uint seed);
bool CanPlantExtraTrees(TileIndex tile);
bool IsTemperateTreeOnSnow(TileIndex tile);


#endif /* TREE_BASE_H */
