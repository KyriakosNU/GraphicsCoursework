#pragma once
#include "../nclgl/OGLRenderer.h"
#include "../nclgl/SceneNode.h"
#include "../nclgl/CubeRobot.h"
class HeightMap;
class Camera;
class Light;
class Shader;

class SurfaceRenderer : public OGLRenderer {
public:
	SurfaceRenderer(Window& parent);
	~SurfaceRenderer(void);

	void RenderScene() override;
	void UpdateScene(float dt) override;
	
protected:
	void AddMeshesToScene();
	void DrawHeightmap();
	void DrawNode(SceneNode* n);
	void DrawSkybox();

	HeightMap* heightMap;
	Shader* shader;
	Shader* skyboxShader;
	Camera* camera;
	Light* light;
	GLuint texture;
	GLuint bumpmap;
	SceneNode* root;
	GLuint cubeMap;
	Mesh* quad;
};

