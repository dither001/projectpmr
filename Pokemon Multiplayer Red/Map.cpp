#include "Map.h"


Map::Map(unsigned char index)
{
	this->index = index;
	tiles = 0;

}

Map::~Map()
{
	if (tiles)
		delete[] tiles;
}

bool Map::Load()
{
	DataBlock* data = ReadFile(ResourceCache::GetResourceLocation(string("maps\\").append(to_string(index).append(".dat"))).c_str());
	if (!ParseHeader(*data))
		return false;
	return true;
}

bool Map::ParseHeader(DataBlock& data)
{
	if (data.size < 3)
		return false;
	
	unsigned char* p = data.data;
	tileset = *p++;
	height = *p++;
	width = *p++;

	tiles = new unsigned char[width * height];
	if (data.size - 3 < (unsigned int)(width * height))
		return true;
	memcpy(tiles, p, width * height);
	p += width * height + 1;
	north.map = *p++;
	north.y_alignment = *p++;
	north.x_alignment = *p++;
	return true;
}