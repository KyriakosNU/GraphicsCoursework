#include "SurfaceRenderer.h"
#include "../nclgl/Light.h"
#include "../nclgl/AutomaticCamera.h"
#include "../nclgl/HeightMap.h"
#include "../nclgl/MeshMaterial.h"
#include "../nclgl/MeshAnimation.h"
#include <algorithm>

#define SHADOWSIZE 2048
const int LIGHT_NUM = 40;
const int POST_PASSES = 10;

SurfaceRenderer::SurfaceRenderer(Window& parent) : OGLRenderer(parent) {
	sphere = Mesh::LoadFromMeshFile("Sphere.msh");
	quad = Mesh::GenerateQuad();

	heightMap = new HeightMap(TEXTUREDIR "PerlinNoise.png" ,32.0f);

	groundText = SOIL_load_OGL_texture(
		TEXTUREDIR "Rock053_2K-JPG_Color.jpg", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	bumpmap = SOIL_load_OGL_texture(
		TEXTUREDIR "Rock053_2K-JPG_NormalGL.jpg", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	cubeMap = SOIL_load_OGL_cubemap(
		TEXTUREDIR "bkg_right.png", TEXTUREDIR "bkg_left.png",
		TEXTUREDIR "bkg_top.png", TEXTUREDIR "bkg_bot.png",
		TEXTUREDIR "bkg_front.png", TEXTUREDIR "bkg_back.png",
		SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 1);
	
	waterTex = SOIL_load_OGL_texture(
		TEXTUREDIR "water.TGA", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	if (!groundText || !bumpmap || !cubeMap ) {
		return;
	}

	SetTextureRepeating(groundText, true);
	SetTextureRepeating(bumpmap, true);
	SetTextureRepeating(waterTex, true);

	sceneShader = new Shader("shadowscenevert.glsl",
		"shadowscenefrag.glsl");
	sceneShader = new Shader("shadowscenevert.glsl", // reused !
		"bufferFragment.glsl");
	skyboxShader = new Shader(
		"skyboxVertex.glsl", "skyboxFragment.glsl");

	skinningShader = new Shader("SkinningVertex.glsl", "texturedFragment.glsl");

	shadowShader = new Shader("shadowVert.glsl", "shadowFrag.glsl");

	pointlightShader = new Shader("pointlightvertex.glsl",
		"pointlightfrag.glsl");

	combineShader = new Shader("combinevert.glsl",
		"combinefrag.glsl");

	processShader = new Shader("TexturedVertex.glsl",
		"processfrag.glsl");

	Shader* TexturedShader = new Shader("TexturedVertex.glsl",
		"TexturedFragment.glsl");

	reflectShader = new Shader(
		"reflectVertex.glsl", "reflectFragment.glsl");

	gammaShader = new Shader("TexturedVertex.glsl","gammaCorrection.glsl");

	bloomShader = new Shader("TexturedVertex.glsl", "bloomFrag.glsl");

	if (!sceneShader->LoadSuccess() ||
		!bloomShader->LoadSuccess()){
		return;
	}

	Vector3 heightmapSize = heightMap->GetHeightmapSize();
	PlanetCenter = Vector3(heightmapSize * Vector3(0.5f, 0.5f, 0.5f));

	camera = new AutomaticCamera();

	light = new Light(PlanetCenter + Vector3(0,4000.0f,0),
		Vector4(0.5f, 0.75f, 0.75f, 1), heightmapSize.x * 0.40f);

	projMatrix = Matrix4::Perspective(1.0f, 15000.0f,
		(float)width / (float)height, 45.0f);
	
	InitShadowMap();
	if(!InitDefShading() || !InitPostProcessing()
		//||!InitAboveView()
		)
		return;

	AddMeshesToScene();

	AddPointLightsToScene();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	waterRotate = 0.0f;
	waterCycle = 0.0f;

	planetTheta = 0.0f;
	planetRotate = 0.0f;

	renderSurface = true;
	renderPostProc = 0;

	UpdatePlanetScale(sky,0);

	init = true;
}

SurfaceRenderer ::~SurfaceRenderer(void) {
	delete camera;
	delete heightMap;
	delete sceneShader;
	delete light;
	delete root;
	delete skyboxShader;
	delete quad;
	delete anim;
	delete mesh;
	delete shadowShader;
	delete processShader;
	delete[] pointLights;
	glDeleteTextures(2, bufferColourTex);
	glDeleteTextures(1, &bufferNormalTex);
	glDeleteTextures(1, &bufferDepthTex);
	glDeleteTextures(1, &lightDiffuseTex);
	glDeleteTextures(1, &lightSpecularTex);

	glDeleteFramebuffers(1, &bufferFBO);
	glDeleteFramebuffers(1, &pointLightFBO);
	glDeleteFramebuffers(1, &processFBO);
	glDeleteTextures(1, &groundText);
	glDeleteTextures(1, &shadowTex);
}

void SurfaceRenderer::InitShadowMap()
{
	glGenTextures(1, &shadowTex);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
		SHADOWSIZE, SHADOWSIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &shadowFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, shadowTex, 0);
	glDrawBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool SurfaceRenderer::InitDefShading() {

	glGenFramebuffers(1, &bufferFBO);
	glGenFramebuffers(1, &pointLightFBO);

	GLenum buffers[2] = {
		GL_COLOR_ATTACHMENT0 ,
		GL_COLOR_ATTACHMENT1
	};

	// Generate our scene depth texture ...
	GenerateScreenTexture(bufferDepthTex, true);
	GenerateScreenTexture(bufferColourTex[0]);
	GenerateScreenTexture(bufferNormalTex);
	GenerateScreenTexture(lightDiffuseTex);
	GenerateScreenTexture(lightSpecularTex);

	// And now attach them to our FBOs
	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, bufferColourTex[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
		GL_TEXTURE_2D, bufferNormalTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, bufferDepthTex, 0);
	glDrawBuffers(2, buffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
		GL_FRAMEBUFFER_COMPLETE) {
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, pointLightFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, lightDiffuseTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
		GL_TEXTURE_2D, lightSpecularTex, 0);
	glDrawBuffers(2, buffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
		GL_FRAMEBUFFER_COMPLETE) {
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_CULL_FACE);
	return true;
}

bool SurfaceRenderer::InitPostProcessing() {
	// Generate our scene depth texture ...
	glGenTextures(1, &bufferDepthTex);
	glBindTexture(GL_TEXTURE_2D, bufferDepthTex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height,
		0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);

	// And our colour texture ...
	for (int i = 0; i < 2; ++i) {
		glGenTextures(1, &bufferColourTex[i]);
		glBindTexture(GL_TEXTURE_2D, bufferColourTex[i]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	}

	glGenFramebuffers(1, &processFBO); // And do post processing in this

	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, bufferDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
		GL_TEXTURE_2D, bufferDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, bufferColourTex[0], 0);
	// We can check FBO attachment success using this command !
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
		GL_FRAMEBUFFER_COMPLETE || !bufferDepthTex || !bufferColourTex[0]) {
		return false;
	}

	return true;
}

void SurfaceRenderer::AddMeshesToScene() {
	Vector3 hSize = heightMap->GetHeightmapSize();

	root = new SceneNode();
	sky = new SceneNode();
	root->AddChild(sky);
	surface = new SceneNode();
	root->AddChild(surface);

	//Adding forest planet nodes
	mesh = Mesh::LoadFromMeshFile("forest.msh");
	material = new MeshMaterial("forest.mat");
	planet = new SceneNode();
	planet->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	planet->SetTransform(
		Matrix4::Translation(Vector3(8400, 200, 8400)));
	planet->SetModelScale(Vector3(200.0f, 200.0f, 200.0f));
	planet->SetBoundingRadius(100.0f);
	planet->SetMesh(mesh);

	for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
		const MeshMaterialEntry* matEntry =
			material->GetMaterialForLayer(i);

		const string* filename = nullptr;
		matEntry->GetEntry("Diffuse", &filename);
		string path = TEXTUREDIR + *filename;
		GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
			SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
		planet->AddTexture(texID);
	}
	
	root->AddChild(planet);

	//Adding forest planet nodes
	mesh = Mesh::LoadFromMeshFile("egipt.msh");
	material = new MeshMaterial("egipt.mat");

	SceneNode* s = new SceneNode();
	s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	s->SetTransform(Matrix4::Translation(
		Vector3(0.0, -2000.0, 0.0)) );
	s->SetModelScale(Vector3(1.0f, 1.0f, 1.0f));
	s->SetBoundingRadius(100.0f);
	s->SetMesh(mesh);

	for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
		const MeshMaterialEntry* matEntry =
			material->GetMaterialForLayer(i);

		const string* filename = nullptr;
		matEntry->GetEntry("Diffuse", &filename);
		string path = TEXTUREDIR + *filename;
		GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
			SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
		s->AddTexture(texID);
	}

	sky->AddChild(s);

	//Adding forest planet nodes
	mesh = Mesh::LoadFromMeshFile("havay.msh");
	material = new MeshMaterial("havay.mat");

	s = new SceneNode();
	s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	s->SetTransform(Matrix4::Translation(
		Vector3(0.0, -2000.0, 0.0)));
	s->SetModelScale(Vector3(1.0f, 1.0f, 1.0f));
	s->SetBoundingRadius(100.0f);
	s->SetMesh(mesh);

	for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
		const MeshMaterialEntry* matEntry =
			material->GetMaterialForLayer(i);

		const string* filename = nullptr;
		matEntry->GetEntry("Diffuse", &filename);
		string path = TEXTUREDIR + *filename;
		GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
			SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
		s->AddTexture(texID);
	}

	sky->AddChild(s);

	//Adding forest planet nodes
	mesh = Mesh::LoadFromMeshFile("pine.msh");
	material = new MeshMaterial("pine.mat");

	s = new SceneNode();
	s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	s->SetTransform(Matrix4::Translation(
		Vector3(0.0, -2000.0, 0.0)));
	s->SetModelScale(Vector3(1.0f, 1.0f, 1.0f));
	s->SetBoundingRadius(100.0f);
	s->SetMesh(mesh);

	for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
		const MeshMaterialEntry* matEntry =
			material->GetMaterialForLayer(i);

		const string* filename = nullptr;
		matEntry->GetEntry("Diffuse", &filename);
		string path = TEXTUREDIR + *filename;
		GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
			SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
		s->AddTexture(texID);
	}

	sky->AddChild(s);

	//Adding Robot Guard

	mesh = Mesh::LoadFromMeshFile("Role_T.msh");
	anim = new MeshAnimation("Role_T.anm");
	material = new MeshMaterial("Role_T.mat");
	 s = new SceneNode();

	s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	s->SetTransform(
		Matrix4::Translation(Vector3(8283.0, 340.0, 9400.0)) * 
		Matrix4::Rotation(180, Vector3(0, 1, 0)));
	s->SetModelScale(Vector3(100.0f, 100.0f, 100.0f));
	s->SetBoundingRadius(100.0f);
	s->SetMesh(mesh);
	s->SetMeshAnimation(anim);

	for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
		const MeshMaterialEntry* matEntry =
			material->GetMaterialForLayer(i);

		const string* filename = nullptr;
		matEntry->GetEntry("Diffuse", &filename);
		string path = TEXTUREDIR + *filename;
		GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
			SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
		s->AddTexture(texID);
	}

	surface->AddChild(s);

	anim = NULL;

	mesh = Mesh::LoadFromMeshFile("BigPlant_05.msh");
	material = new MeshMaterial("BigPlant_05.mat");
	s = new SceneNode();
	s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	s->SetTransform(Matrix4::Translation(
		Vector3(9178.0, 302.0, 6731.0)) * Matrix4::Rotation(180, Vector3(0, 1, 0)));
	s->SetModelScale(Vector3(50.0f, 50.0f, 50.0f));
	s->SetBoundingRadius(100.0f);
	s->SetMesh(mesh);

	for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
		const MeshMaterialEntry* matEntry =
			material->GetMaterialForLayer(i);

		const string* filename = nullptr;
		matEntry->GetEntry("Diffuse", &filename);
		string path = TEXTUREDIR + *filename;
		GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
			SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
		s->AddTexture(texID);
	}

	surface->AddChild(s);

	mesh = Mesh::LoadFromMeshFile("Grass_01.msh");
	material = new MeshMaterial("Grass_01.mat");
	for (int i = 0; i < 5; i++) {
		s = new SceneNode();
		s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));

		s->SetTransform(Matrix4::Translation(
			Vector3(float(8378.0 + 520 * (float)(rand() / (float)RAND_MAX)),
				252.0,
				float(6031.0 + 1520 * (float)(rand() / (float)RAND_MAX)))) *
			Matrix4::Rotation(360 * (float)(rand() / (float)RAND_MAX)
				, Vector3(0, 1, 0)));

		s->SetModelScale(Vector3(150.0f, 150.0f, 150.0f));
		s->SetBoundingRadius(0.0f);
		s->SetMesh(mesh);

		for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
			const MeshMaterialEntry* matEntry =
				material->GetMaterialForLayer(i);

			const string* filename = nullptr;
			matEntry->GetEntry("Diffuse", &filename);
			string path = TEXTUREDIR + *filename;
			GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
				SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
			s->AddTexture(texID);
		}

		surface->AddChild(s);
	}



	mesh = Mesh::LoadFromMeshFile("SpaceshipDiffuse.msh");
	material = new MeshMaterial("SpaceshipDiffuse.mat");
	s = new SceneNode();
	s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	s->SetTransform(Matrix4::Translation(Vector3(8283.0, 400.0, 9400.0)) * 
					Matrix4::Rotation(15,Vector3(0,1,0)));
	s->SetModelScale(Vector3(200.0f, 200.0f, 200.0f));
	s->SetBoundingRadius(0.0f);
	s->SetMesh(mesh);

	for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
		const MeshMaterialEntry* matEntry =
			material->GetMaterialForLayer(i);

		const string* filename = nullptr;
		matEntry->GetEntry("Diffuse", &filename);
		string path = TEXTUREDIR + *filename;
		GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
			SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
		s->AddTexture(texID);
	}


	surface->AddChild(s);

	mesh = Mesh::LoadFromMeshFile("Mushrooms.msh");
	material = new MeshMaterial("Mushrooms.mat");
	for (int i = 0; i < 7; i++){
			s = new SceneNode();
		s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));

		s->SetTransform(Matrix4::Translation(
			Vector3(float(7032.0 + 2420 * (float)(rand() / (float)RAND_MAX)),
				320.0,
				float(9320.0 + 2020 * (float)(rand() / (float)RAND_MAX)))) *
			 Matrix4::Rotation(360 *(float)(rand() / (float)RAND_MAX)
				, Vector3(0, 1, 0)));

		float randomScale = 30.0f + (170) * (float)(rand() / (float)RAND_MAX);

		s->SetModelScale(Vector3(randomScale, randomScale, randomScale));
		s->SetBoundingRadius(0.0f);
		s->SetMesh(mesh);

		for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
			const MeshMaterialEntry* matEntry =
				material->GetMaterialForLayer(i);

			const string* filename = nullptr;
			matEntry->GetEntry("Diffuse", &filename);
			string path = TEXTUREDIR + *filename;
			GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
				SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
			s->AddTexture(texID);
		}

		surface->AddChild(s);
	}

	mesh = Mesh::LoadFromMeshFile("BigPlant_03.msh");
	material = new MeshMaterial("BigPlant_03.mat");
	for (int i = 0; i < 7; i++) {
		s = new SceneNode();
		s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));

		s->SetTransform(Matrix4::Translation(
			Vector3(float(7275.0 + 820 * (float)(rand() / (float)RAND_MAX)),
				200.0,
				float(6935.0 + 1520 * (float)(rand() / (float)RAND_MAX)))) *
			Matrix4::Rotation(360 * (float)(rand() / (float)RAND_MAX)
				, Vector3(0, 1, 0)));

		float randomScale = 1.0f + (60) * (float)(rand() / (float)RAND_MAX);

		s->SetModelScale(Vector3(randomScale, randomScale, randomScale));
		s->SetBoundingRadius(0.0f);
		s->SetMesh(mesh);

		for (int i = 0; i < mesh->GetSubMeshCount(); ++i) {
			const MeshMaterialEntry* matEntry =
				material->GetMaterialForLayer(i);

			const string* filename = nullptr;
			matEntry->GetEntry("Diffuse", &filename);
			string path = TEXTUREDIR + *filename;
			GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO,
				SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
			s->AddTexture(texID);
		}

		surface->AddChild(s);
	}

	BuildSkyNodeLists(sky);
}

void SurfaceRenderer::AddPointLightsToScene(){
	Vector3 heightmapSize = heightMap->GetHeightmapSize();
	pointLights = new Light[LIGHT_NUM];

	for (int i = 1; i < LIGHT_NUM; ++i) {
		Light& l = pointLights[i];
		l.SetPosition(Vector3(rand() % (int)heightmapSize.x,
			350.0f,
			rand() % (int)heightmapSize.z));

		l.SetColour(Vector4(0.65f + (float)(rand() / (float)RAND_MAX),
			0.25f + (float)(rand() / (float)RAND_MAX),
			0.65f + (float)(rand() / (float)RAND_MAX),
			1));
		l.SetRadius(700.0f + (rand() % 250));
	}

	pointLights[0] = *light;
}

void SurfaceRenderer::UpdateScene(float dt) {
	camera->UpdateCamera(dt);

	projMatrix = Matrix4::Perspective(1.0f, 15000.0f,
		(float)width / (float)height, 45.0f);

	viewMatrix = camera->BuildViewMatrix();
	frameFrustum.FromMatrix(projMatrix * viewMatrix);

	root->Update(dt);

	waterRotate += dt * 0.40f; //2 degrees a second
	waterCycle += dt * 0.0005f; // 10 units a second

	planetTheta += dt * 0.1f; // 10 units a second
	planetRotate += dt * 5.0f;

	UpdatePlanet(sky, 0);

	if (camera->getTimeElapsed() > 20 &&
		camera->getCurrentTrack()==2 &&
		renderSurface)
	{
		renderSurface = false;
		UpdatePlanetScale(sky, 0);
	}
		
	if (camera->getTimeElapsed()<=1 &&
		!renderSurface)
	{ 
		renderSurface = true;
		UpdatePlanetScale(sky, 0);
	}

	if (camera->getTimeElapsed() > 0 &&
		!camera->getIsFree() && 
		camera->getCurrentTrack() == 2)
	{
		renderPostProc = 2;
	}

	if (camera->getTimeElapsed() > 0 &&
		!camera->getIsFree() &&
		camera->getCurrentTrack() == 1)
	{
		renderPostProc = 3;
	}

	if (Window::GetKeyboard()->KeyDown(KEYBOARD_3) &&
		!Window::GetKeyboard()->KeyHeld(KEYBOARD_3))
		renderPostProc = (renderPostProc<4) ? renderPostProc +1 : 0;
}

void SurfaceRenderer::RenderScene() {
	if (renderSurface)
		BuildNodeLists(surface);

	SortNodeLists();

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	
	DrawSkybox();

	if (renderSurface)
	{
		DrawHeightmap(camera->BuildViewMatrix());
		DrawWater(&camera->GetPosition());
	}
	else
	{
		DrawNode(planet);
	}
	DrawNodes();
	DrawPointLights();
	CombineBuffers();

	switch (renderPostProc)
	{ 
		case 0:
			break;
		case 1:
			DrawPostProcess();
			break;
		case 2:
			DrawBloom(true);
			break;
		case 3:
			DrawBloom(false);
			break;
		case 4:
			DrawGammaCorrection();
			break;
	}
	PresentScene();
	if (renderSurface)
		DrawAboveView();

	ClearNodeLists();
}

void SurfaceRenderer::UpdatePlanet(SceneNode* from,int c)
{
	float rScale = (renderSurface) ? 1 : 0.5f;
	switch (c){
		case 0:
			break;
		case 1:
			from->SetTransform(
				Matrix4::Translation(PlanetCenter+
					Vector3(rScale * 13000*cos(planetTheta),
							0,
							rScale * 13000*sin(planetTheta)))*
				Matrix4::Rotation(planetRotate
							, Vector3(1, 0, 1)));

			break;
		case 2:
			from->SetTransform(
				Matrix4::Translation(PlanetCenter +
					Vector3(rScale * 9200 * cos(planetTheta - 30),
						rScale * 9200 * sin(planetTheta - 30),
						0.0f)) *
				Matrix4::Rotation(planetRotate
					, Vector3(1, 1, 0)));
			break;
		case 3:
			from->SetTransform(
				Matrix4::Translation(PlanetCenter +
					Vector3(0.0f,
						rScale * 9400 * sin(planetTheta + 80),
						rScale * 9400 * cos(planetTheta + 80))) *
				Matrix4::Rotation(planetRotate
					, Vector3(0, 1, 1)));
			break;
	}
	
	for (vector < SceneNode* >::const_iterator i =
		from->GetChildIteratorStart();
		i != from->GetChildIteratorEnd(); ++i) {
		c++;
		UpdatePlanet((*i),c);
	}
}

void SurfaceRenderer::UpdatePlanetScale(SceneNode* from, int c)
{
	float scaleFactor = (renderSurface) ? 1.0f : 0.5f;
	switch (c) {
	case 0:
		break;
	case 1:
		from->SetModelScale(Vector3(155,155,155) * scaleFactor);
		break;
	case 2:
		from->SetModelScale(Vector3(170, 170, 170) * scaleFactor);
		break;
	case 3:
		from->SetModelScale(Vector3(120, 120, 120) * scaleFactor);
		break;
	}

	for (vector < SceneNode* >::const_iterator i =
		from->GetChildIteratorStart();
		i != from->GetChildIteratorEnd(); ++i) {
		c++;
		UpdatePlanetScale((*i), c);
	}
}


void SurfaceRenderer::BuildNodeLists(SceneNode* from) {
	if (frameFrustum.InsideFrustum(*from)) {
		Vector3 dir = from->GetWorldTransform().GetPositionVector() -
			camera->GetPosition();
		from->SetCameraDistance(Vector3::Dot(dir, dir));

		if (from->GetColour().w < 1.0f) {
			transparentNodeList.push_back(from);
		}
		else {
			nodeList.push_back(from);
		}

	}

	for (vector < SceneNode* >::const_iterator i =
		from->GetChildIteratorStart();
		i != from->GetChildIteratorEnd(); ++i) {
		BuildNodeLists((*i));
	}

}

void SurfaceRenderer::BuildSkyNodeLists(SceneNode* from) {

	Vector3 dir = from->GetWorldTransform().GetPositionVector() -
		camera->GetPosition();
	from->SetCameraDistance(Vector3::Dot(dir, dir));

	if (from->GetColour().w < 1.0f) {
		transparentSkyNodeList.push_back(from);
	}
	else {
		skyNodeList.push_back(from);
	}

	for (vector < SceneNode* >::const_iterator i =
		from->GetChildIteratorStart();
		i != from->GetChildIteratorEnd(); ++i) {
		BuildSkyNodeLists((*i));
	}

}

void SurfaceRenderer::SortNodeLists() {
	std::sort(transparentNodeList.rbegin(), 
		transparentNodeList.rend(), 
		SceneNode::CompareByCameraDistance);

	std::sort(transparentSkyNodeList.rbegin(),
		transparentSkyNodeList.rend(),
		SceneNode::CompareByCameraDistance);

	std::sort(nodeList.begin(),
		nodeList.end(),
		SceneNode::CompareByCameraDistance);

	std::sort(skyNodeList.begin(),
		skyNodeList.end(),
		SceneNode::CompareByCameraDistance);
}

void SurfaceRenderer::DrawSkybox() {

	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glDepthMask(GL_FALSE);

	BindShader(skyboxShader);
	UpdateShaderMatrices();

	quad->Draw();

	glDepthMask(GL_TRUE);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SurfaceRenderer::DrawHeightmap(Matrix4 ViewMatrix){

	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);

	BindShader(sceneShader);
	viewMatrix = ViewMatrix;
	projMatrix = Matrix4::Perspective(1.0f, 15000.0f,
		(float)width / (float)height, 45.0f);

	glUniform1i(glGetUniformLocation(
		sceneShader->GetProgram(), "diffuseTex"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, groundText);

	glUniform1i(glGetUniformLocation(
		sceneShader->GetProgram(), "bumpTex"), 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, bumpmap);
	
	glUniform1i(glGetUniformLocation(sceneShader->GetProgram(),
		"shadowTex"), 2);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	
	glUniform3fv(glGetUniformLocation(sceneShader->GetProgram(),
		"cameraPos"), 1, (float*)&camera->GetPosition());

	modelMatrix.ToIdentity();
	textureMatrix.ToIdentity();

	UpdateShaderMatrices();
	SetShaderLight(*light);

	heightMap->Draw();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SurfaceRenderer::DrawWater(Vector3* CameraPos) {
	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);
	BindShader(reflectShader);

	glUniform3fv(glGetUniformLocation(reflectShader->GetProgram(),
		"cameraPos"), 1, (float*)CameraPos);

	glUniform1i(glGetUniformLocation(
		reflectShader->GetProgram(), "diffuseTex"), 0);
	glUniform1i(glGetUniformLocation(
		reflectShader->GetProgram(), "cubeTex"), 2);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, waterTex);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);

	Vector3 hSize = heightMap->GetHeightmapSize();

	modelMatrix =
		Matrix4::Translation(hSize * 0.5f + Vector3(0,-20,0)) *
		Matrix4::Scale(hSize * 0.28f) *
		Matrix4::Rotation(-90, Vector3(1, 0, 0));

	textureMatrix =
		Matrix4::Translation(Vector3(waterCycle, 0.0f, waterCycle)) *
		Matrix4::Scale(Vector3(10, 10, 10)) *
		Matrix4::Rotation(waterRotate, Vector3(0, 0, 1));

	UpdateShaderMatrices();
	// SetShaderLight (* light ); // No lighting in this shader !
	quad->Draw();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



void SurfaceRenderer::DrawNodes() {

	
	for (const auto& i : nodeList) {
		if (!i->GetMeshAnimation())
			DrawNode(i);
		else
			DrawAnimatedNode(i);
	}

	for (const auto& i : skyNodeList) {
		if (!i->GetMeshAnimation())
			DrawNode(i);
		else
			DrawAnimatedNode(i);
	}

	for (const auto& i : transparentNodeList) {
		if (!i->GetMeshAnimation())
			DrawNode(i);
		else
			DrawAnimatedNode(i);
	}

	for (const auto& i : transparentSkyNodeList) {
		if (!i->GetMeshAnimation())
			DrawNode(i);
		else
			DrawAnimatedNode(i);
	}
}

void SurfaceRenderer::DrawNode(SceneNode* n) {
	if (n->GetMesh()) {
		BindShader(sceneShader);
		UpdateShaderMatrices();
		glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);

		Matrix4 model = n->GetWorldTransform() *
			Matrix4::Scale(n->GetModelScale());

		glUniformMatrix4fv(
			glGetUniformLocation(sceneShader->GetProgram(),
				"modelMatrix"), 1, false, model.values);

		glUniform4fv(glGetUniformLocation(sceneShader->GetProgram(),
			"nodeColour"), 1, (float*)&n->GetColour());

		glUniform1i(glGetUniformLocation(
			sceneShader->GetProgram(), "bumpTex"), 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, bumpmap);

		for (int i = 0; i < n->GetMesh()->GetSubMeshCount(); ++i) {
			texture = n->GetTexture(i);
			
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);

			glUniform1i(glGetUniformLocation(sceneShader->GetProgram(),
				"useTexture"), texture);
			modelMatrix = model;
			UpdateShaderMatrices();
			n->GetMesh()->DrawSubMesh(i);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

void SurfaceRenderer::DrawAnimatedNode(SceneNode* n)
{
	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);
	BindShader(skinningShader);
	glUniform1i(glGetUniformLocation(skinningShader->GetProgram(),
		"diffuseTex"), 0);

	UpdateShaderMatrices();
	Matrix4 model = n->GetWorldTransform() *
		Matrix4::Scale(n->GetModelScale());

	glUniformMatrix4fv(
		glGetUniformLocation(skinningShader->GetProgram(),
			"modelMatrix"), 1, false, model.values);

	vector < Matrix4 > frameMatrices;
	
	const Matrix4* invBindPose = n->GetMesh()->GetInverseBindPose();
	const Matrix4* frameData = n->GetFrameData();

	for (unsigned int i = 0; i < n->GetMesh()->GetJointCount(); ++i) {
		frameMatrices.emplace_back(frameData[i] * invBindPose[i]);
	}

	int j = glGetUniformLocation(skinningShader->GetProgram(), "joints");
	glUniformMatrix4fv(j, frameMatrices.size(), false,
		(float*)frameMatrices.data());

	for (int i = 0; i < n->GetMesh()->GetSubMeshCount(); ++i) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, n->GetTexture(i));
		n->GetMesh()->DrawSubMesh(i);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SurfaceRenderer::ClearNodeLists() {
	transparentNodeList.clear();
	nodeList.clear();
}

void SurfaceRenderer::GenerateScreenTexture(GLuint& into, bool depth) {
	glGenTextures(1, &into);
	glBindTexture(GL_TEXTURE_2D, into);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	GLuint format = depth ? GL_DEPTH_COMPONENT24 : GL_RGBA8;
	GLuint type = depth ? GL_DEPTH_COMPONENT : GL_RGBA;

	glTexImage2D(GL_TEXTURE_2D, 0,
		format, width, height, 0, type, GL_UNSIGNED_BYTE, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void SurfaceRenderer::DrawPointLights() {
	glBindFramebuffer(GL_FRAMEBUFFER, pointLightFBO);
	BindShader(pointlightShader);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glBlendFunc(GL_ONE, GL_ONE);
	glCullFace(GL_FRONT);
	glDepthFunc(GL_ALWAYS);
	glDepthMask(GL_FALSE);

	glUniform1i(glGetUniformLocation(
		pointlightShader->GetProgram(), "depthTex"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, bufferDepthTex);

	glUniform1i(glGetUniformLocation(
		pointlightShader->GetProgram(), "normTex"), 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, bufferNormalTex);

	glUniform3fv(glGetUniformLocation(pointlightShader->GetProgram(),
		"cameraPos"), 1, (float*)&camera->GetPosition());

	glUniform2f(glGetUniformLocation(pointlightShader->GetProgram(),
		"pixelSize"), 1.0f / width, 1.0f / height);

	Matrix4 invViewProj = (projMatrix * viewMatrix).Inverse();
	glUniformMatrix4fv(glGetUniformLocation(
		pointlightShader->GetProgram(), "inverseProjView"),
		1, false, invViewProj.values);

	UpdateShaderMatrices();
	if (renderSurface)
	{
		for (int i = 0; i < LIGHT_NUM; ++i) {
			Light& l = pointLights[i];
			SetShaderLight(l);
			sphere->Draw();
		}
	}
	else {
		SetShaderLight(pointLights[0]);
		sphere->Draw();
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LEQUAL);

	glDepthMask(GL_TRUE);

	glClearColor(0.2f, 0.2f, 0.2f, 1);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SurfaceRenderer::DrawPostProcess() {
	glBindFramebuffer(GL_FRAMEBUFFER, processFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, bufferColourTex[1], 0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	BindShader(processShader);
	modelMatrix.ToIdentity();
	viewMatrix.ToIdentity();
	projMatrix.ToIdentity();
	textureMatrix.ToIdentity();
	UpdateShaderMatrices();

	glDisable(GL_DEPTH_TEST);

	glActiveTexture(GL_TEXTURE0);
	glUniform1i(glGetUniformLocation(
		processShader->GetProgram(), "sceneTex"), 0);
	for (int i = 0; i < POST_PASSES; ++i) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, bufferColourTex[1], 0);
		glUniform1i(glGetUniformLocation(processShader->GetProgram(),
			"isVertical"), 0);

		glBindTexture(GL_TEXTURE_2D, bufferColourTex[0]);
		quad->Draw();
		// Now to swap the colour buffers , and do the second blur pass
		glUniform1i(glGetUniformLocation(processShader->GetProgram(),
			"isVertical"), 1);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, bufferColourTex[0], 0);
		glBindTexture(GL_TEXTURE_2D, bufferColourTex[1]);
		quad->Draw();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void SurfaceRenderer::DrawGammaCorrection() {
	glBindFramebuffer(GL_FRAMEBUFFER, processFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, bufferColourTex[1], 0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	BindShader(gammaShader);
	modelMatrix.ToIdentity();
	viewMatrix.ToIdentity();
	projMatrix.ToIdentity();
	textureMatrix.ToIdentity();
	UpdateShaderMatrices();

	glDisable(GL_DEPTH_TEST);

	glActiveTexture(GL_TEXTURE0);
	glUniform1i(glGetUniformLocation(
		gammaShader->GetProgram(), "sceneTex"), 0);
	
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, bufferColourTex[0], 0);
	glBindTexture(GL_TEXTURE_2D, bufferColourTex[0]);
	quad->Draw();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void SurfaceRenderer::DrawBloom(bool isInverted) {
	glBindFramebuffer(GL_FRAMEBUFFER, processFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, bufferColourTex[1], 0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	BindShader(bloomShader);
	modelMatrix.ToIdentity();
	viewMatrix.ToIdentity();
	projMatrix.ToIdentity();
	textureMatrix.ToIdentity();
	UpdateShaderMatrices();

	glDisable(GL_DEPTH_TEST);

	glActiveTexture(GL_TEXTURE0);
	glUniform1i(glGetUniformLocation(
		bloomShader->GetProgram(), "sceneTex"), 0);

	glUniform1i(glGetUniformLocation(
		bloomShader->GetProgram(), "isInverted"), isInverted);
	for (int i = 0; i < POST_PASSES; ++i) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, bufferColourTex[1], 0);
		glUniform1i(glGetUniformLocation(bloomShader->GetProgram(),
			"isVertical"), 0);

		glBindTexture(GL_TEXTURE_2D, bufferColourTex[0]);
		quad->Draw();
		// Now to swap the colour buffers , and do the second blur pass
		glUniform1i(glGetUniformLocation(bloomShader->GetProgram(),
			"isVertical"), 1);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, bufferColourTex[0], 0);
		glBindTexture(GL_TEXTURE_2D, bufferColourTex[1]);
		quad->Draw();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void SurfaceRenderer::CombineBuffers() {
	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);
	BindShader(combineShader);
	modelMatrix.ToIdentity();
	viewMatrix.ToIdentity();
	projMatrix.ToIdentity();
	UpdateShaderMatrices();

	glUniform1i(glGetUniformLocation(
		combineShader->GetProgram(), "diffuseTex"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, bufferColourTex[0]);

	glUniform1i(glGetUniformLocation(
		combineShader->GetProgram(), "diffuseLight"), 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, lightDiffuseTex);

	glUniform1i(glGetUniformLocation(
		combineShader->GetProgram(), "specularLight"), 2);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, lightSpecularTex);

	quad->Draw();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SurfaceRenderer::PresentScene() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	BindShader(sceneShader);
	modelMatrix.ToIdentity();
	viewMatrix.ToIdentity();
	projMatrix.ToIdentity();
	UpdateShaderMatrices();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, bufferColourTex[0]);
	glUniform1i(glGetUniformLocation(sceneShader->GetProgram(),
		"diffuseTex"), 0);
	glUniform1i(glGetUniformLocation(sceneShader->GetProgram(),
		"bumpTex"), 1);
	glUniform1i(glGetUniformLocation(sceneShader->GetProgram(),
		"shadowTex"), 2);
	quad->Draw();
}

void SurfaceRenderer::DrawAboveView(){
	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	Vector3 AboveCameraPos = -Vector3(camera->GetPosition().x,
		3000,
		camera->GetPosition().z);
	viewMatrix = Matrix4::Rotation(90, Vector3(1, 0, 0)) *
		Matrix4::Translation(AboveCameraPos);


		DrawHeightmap(Matrix4::Rotation(90, Vector3(1, 0, 0)) *
			Matrix4::Translation(AboveCameraPos)); 
		DrawWater(&AboveCameraPos);

	DrawPointLights();
	CombineBuffers();
	glViewport(height / 64, height / 64, width / 4, height / 4);
	PresentScene();
	glViewport(0, 0, width, height);
}