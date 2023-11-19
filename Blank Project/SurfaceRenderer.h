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
	void BuildSkyNodeLists(SceneNode* from);
	void AddMeshesToScene();
	void AddPointLightsToScene();
	void DrawHeightmap(Matrix4 viewmatrix);
	void DrawNodes();
	void DrawNode(SceneNode* n);
	void DrawAnimatedNode(SceneNode* n);
	void DrawSkybox();
	void DrawWater(Vector3* CameraPos);
	void DrawAboveView();
	void SortNodeLists();
	void ClearNodeLists();
	void InitShadowMap();
	bool InitDefShading();
	bool InitPostProcessing();
	void DrawPointLights(); // Lighting Render Pass
	void CombineBuffers(); // Combination Render Pass
	void GenerateScreenTexture(GLuint& into, bool depth = false);
	void DrawPostProcess();
	void DrawGammaCorrection();
	void DrawBloom(bool isInverted);
	void PresentScene();
	void UpdatePlanet(SceneNode* from,int c);
	void UpdatePlanetScale(SceneNode* from, int c);

	bool renderSurface;
	int renderPostProc;
	AutomaticCamera* camera;

	Shader* sceneShader;
	Shader* skyboxShader;
	Shader* skinningShader;
	Shader* processShader;
	Shader* shadowShader;
	Shader* pointlightShader;
	Shader* combineShader;
	Shader* texturedShader;
	Shader* reflectShader;
	Shader* gammaShader;
	Shader* bloomShader;

	HeightMap* heightMap;
	
	Light* light;

	GLuint waterTex;
	GLuint texture;
	GLuint groundText;
	GLuint bumpmap;
	GLuint cubeMap;

	Frustum frameFrustum;

	Mesh* quad;
	Mesh* mesh;
	

	SceneNode* root;
	SceneNode* surface;
	SceneNode* sky;
	SceneNode* planet;

	MeshAnimation* anim;
	MeshMaterial* material;
	vector <GLuint > matTextures;

	vector < SceneNode*> transparentNodeList;
	vector < SceneNode*> nodeList;
	vector < SceneNode*> skyNodeList;
	vector < SceneNode*> transparentSkyNodeList;

	GLuint shadowTex;
	GLuint shadowFBO;
	GLuint combineFBO;


	GLuint bufferFBO; 
	GLuint bufferColourTex[2]; 
	GLuint bufferNormalTex;
	GLuint bufferDepthTex; 

	GLuint pointLightFBO;
	GLuint lightDiffuseTex;
	GLuint lightSpecularTex;
	Light* pointLights;
	Mesh* sphere; 

	GLuint processFBO;

	float waterRotate;
	float waterCycle;

	float planetRotate;
	float planetTheta;

	Vector3 PlanetCenter;
};

