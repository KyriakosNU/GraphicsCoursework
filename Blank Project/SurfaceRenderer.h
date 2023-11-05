#pragma once
#include "../nclgl/OGLRenderer.h"
class HeightMap;
class Camera;
class Light; // Predeclare our new class type ...
class Shader;

class SurfaceRenderer : public OGLRenderer {
public:
	SurfaceRenderer(Window& parent);
	~SurfaceRenderer(void);

	void RenderScene() override;
	void UpdateScene(float dt) override;

protected:
	HeightMap* heightMap;
	Shader* shader;
	Camera* camera;
	Light* light;
	GLuint texture;
	GLuint bumpmap;
};

