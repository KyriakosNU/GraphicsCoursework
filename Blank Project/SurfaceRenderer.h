#pragma once
#include "../nclgl/OGLRenderer.h"
#include "../nclgl/SceneNode.h"
#include "../nclgl/CubeRobot.h"
#include "../nclgl/Frustum.h"
class HeightMap;
class Camera;
class Light;
class Shader;
class MeshAnimation;
class MeshMaterial;

class SurfaceRenderer : public OGLRenderer {
public:
	SurfaceRenderer(Window& parent);
	~SurfaceRenderer(void);

	void RenderScene() override;
	void UpdateScene(float dt) override;
	
protected:
	void BuildNodeLists(SceneNode* from);
	void AddMeshesToScene();
	void DrawHeightmap();
	void DrawNodes();
	void DrawNode(SceneNode* n);
	void DrawSkybox();
	void SortNodeLists();
	void ClearNodeLists();

	Camera* camera;

	Shader* shader;
	Shader* skyboxShader;

	HeightMap* heightMap;
	
	Light* light;

	GLuint texture;
	GLuint groundText;
	GLuint bumpmap;
	GLuint cubeMap;

	vector <GLuint > matTextures;

	Frustum frameFrustum;

	Mesh* quad;
	Mesh* planetMesh;

	SceneNode* root;

	MeshAnimation* anim;
	MeshMaterial* material;

	vector < SceneNode*> transparentNodeList;
	vector < SceneNode*> nodeList;
};

