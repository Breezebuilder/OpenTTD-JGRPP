/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file raster_import.h Functions related to modification of maps from multiple raster data types. */

#ifndef RASTER_IMPORT_H
#define RASTER_IMPORT_H

#include "fileio_type.h"
#include "raster_io.h"

void LoadRaster(DetailedFileType dft, RasterDataType rdt, const char *filename, Subdirectory subdir = GEOMAP_DIR);

#endif /* RASTER_IMPORT_H */
