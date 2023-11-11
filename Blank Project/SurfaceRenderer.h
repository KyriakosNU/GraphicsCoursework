#pragma once
#include "../nclgl/OGLRenderer.h"
#include "../nclgl/SceneNode.h"
#include "../nclgl/CubeRobot.h"
#include "../nclgl/Frustum.h"
class HeightMap;
class AutomaticCamera;
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
	void DrawNodeShadows();
	void DrawNode(SceneNode* n);
	void DrawNodeShadow(SceneNode* n);
	void DrawAnimatedNode(SceneNode* n);
	void DrawSkybox();
	void DrawShadowScene();
	void SortNodeLists();
	void ClearNodeLists();
	void ShadowMapInit();

	AutomaticCamera* camera;

	Shader* sceneShader;
	Shader* skyboxShader;
	Shader* skinningShader;

	HeightMap* heightMap;
	
	Light* light;

	GLuint texture;
	GLuint groundText;
	GLuint bumpmap;
	GLuint cubeMap;

	Frustum frameFrustum;

	Mesh* quad;
	Mesh* mesh;

	SceneNode* root;

	MeshAnimation* anim;
	MeshMaterial* material;
	vector <GLuint > matTextures;

	vector < SceneNode*> animatedNodeList;
	vector < SceneNode*> transparentNodeList;
	vector < SceneNode*> nodeList;

	GLuint shadowTex;
	GLuint shadowFBO;

	Shader* shadowShader;
};

