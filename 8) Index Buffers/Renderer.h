#pragma once
#include "../nclgl/OGLRenderer.h"
class HeightMap;
class Camera;

class Renderer :
    public OGLRenderer
{
public:
	Renderer(Window& parent);
	~Renderer(void);

	void UpdateScene(float msec) override;
	void RenderScene() override;
protected:
	HeightMap* heightMap;
	Shader* shader;
	Camera* camera;
	GLuint terrainTex;
};

