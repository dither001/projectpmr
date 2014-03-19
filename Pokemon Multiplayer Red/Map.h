#pragma once

#include "Common.h"
#include "TileMap.h"
#include "Utils.h"
#include "ResourceCache.h"
#include "DataBlock.h"
#include "MapConnection.h"

class Map
{
public:
	Map(unsigned char index);
	~Map();

	bool Load();

	unsigned char index;
	unsigned char width;
	unsigned char height;
	unsigned char tileset;
	unsigned char border_tile;

	unsigned char* tiles;
	MapConnection north;

private:

	bool ParseHeader(DataBlock& data);
};
