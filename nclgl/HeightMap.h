#pragma once
#include <string>
#include "Mesh.h"

class HeightMap :
    public Mesh
{
public:
    HeightMap(const std::string& name);
    HeightMap(const std::string& name, const float scale);
    ~HeightMap(void) {};

    Vector3 GetHeightmapSize() const{ return heightmapSize; };
protected:
    Vector3 heightmapSize;
};

