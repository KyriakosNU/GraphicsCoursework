#include "SurfaceRenderer.h"
#include "../nclgl/Light.h"
#include "../nclgl/AutomaticCamera.h"
#include "../nclgl/HeightMap.h"
#include "../nclgl/MeshMaterial.h"
#include "../nclgl/MeshAnimation.h"
#include <algorithm>

#define SHADOWSIZE 2048
const int LIGHT_NUM = 32;
const int POST_PASSES = 10;

SurfaceRenderer::SurfaceRenderer(Window& parent) : OGLRenderer(parent) {
	sphere = Mesh::LoadFromMeshFile("Sphere.msh");
	quad = Mesh::GenerateQuad();

	heightMap = new HeightMap(TEXTUREDIR "noise.png" , 32.0f);

	groundText = SOIL_load_OGL_texture(
		TEXTUREDIR "Barren Reds.JPG ", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	bumpmap = SOIL_load_OGL_texture(
		TEXTUREDIR "Barren RedsDOT3.JPG ", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	cubeMap = SOIL_load_OGL_cubemap(
		TEXTUREDIR "bkg_right.png", TEXTUREDIR "bkg_left.png",
		TEXTUREDIR "bkg_top.png", TEXTUREDIR "bkg_bot.png",
		TEXTUREDIR "bkg_front.png", TEXTUREDIR "bkg_back.png",
		SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 1);
	
	if (!groundText || !bumpmap || !cubeMap ) {
		return;
	}

	SetTextureRepeating(groundText, true);
	SetTextureRepeating(bumpmap, true);

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

	if (!sceneShader->LoadSuccess() ||
		!skyboxShader->LoadSuccess()){
		return;
	}

	Vector3 heightmapSize = heightMap->GetHeightmapSize();

	camera = new AutomaticCamera(-45.0f, 0.0f,
		heightmapSize * Vector3(0.5f, 5.0f, 0.5f),false);

	light = new Light(heightmapSize * Vector3(0.5f,1.5f, 0.5f),
		Vector4(1, 1, 1, 1), heightmapSize.x * 0.5f);

	projMatrix = Matrix4::Perspective(1.0f, 15000.0f,
		(float)width / (float)height, 45.0f);
	
	//InitShadowMap();
	if(!InitDefShading() || !InitPostProcessing())
		return;

	AddMeshesToScene();
	AddPointLightsToScene();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);


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
void SurfaceRenderer::AddMeshesToScene(){
	Mesh* cube = Mesh::LoadFromMeshFile("OffsetCubeY.msh");
	//CubeRobot* CR = new CubeRobot(cube);
	//CR->SetTransform(Matrix4::Translation(Vector3(4296.0, 400.0, 4096.0)));
	root = new SceneNode();
	//root->AddChild(CR);

	//Adding forest planet nodes
	mesh = Mesh::LoadFromMeshFile("forest.msh");
	material = new MeshMaterial("forest.mat");

	for (int i = 0; i < 1; ++i) {
		SceneNode* s = new SceneNode();
		s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		s->SetTransform(Matrix4::Translation(
			Vector3(4296.0, 500.0, 5096.0)) * Matrix4::Rotation(180, Vector3(0, 1, 0)));
		s->SetModelScale(Vector3(20.0f, 20.0f, 20.0f));
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

		root->AddChild(s);
	}

	//Adding Robot Guard

	mesh = Mesh::LoadFromMeshFile("Role_T.msh");
	anim = new MeshAnimation("Role_T.anm");
	material = new MeshMaterial("Role_T.mat");
	SceneNode* s = new SceneNode();

	s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	s->SetTransform(Matrix4::Translation(
		Vector3(4300.0, 500.0, 5000.0)) * Matrix4::Rotation(180, Vector3(0, 1, 0)));
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

	root->AddChild(s);

}

void SurfaceRenderer::AddPointLightsToScene(){
	Vector3 heightmapSize = heightMap->GetHeightmapSize();
	pointLights = new Light[LIGHT_NUM];

	for (int i = 0; i < LIGHT_NUM-1; ++i) {
		Light& l = pointLights[i];
		l.SetPosition(Vector3(rand() % (int)heightmapSize.x,
			150.0f,
			rand() % (int)heightmapSize.z));

		l.SetColour(Vector4(0.5f + (float)(rand() / (float)RAND_MAX),
			0.5f + (float)(rand() / (float)RAND_MAX),
			0.5f + (float)(rand() / (float)RAND_MAX),
			1));
		l.SetRadius(250.0f + (rand() % 250));
	}

	pointLights[LIGHT_NUM - 1] = *light;
}

void SurfaceRenderer::UpdateScene(float dt) {
	camera->UpdateCamera(dt);

	projMatrix = Matrix4::Perspective(1.0f, 15000.0f,
		(float)width / (float)height, 45.0f);

	viewMatrix = camera->BuildViewMatrix();
	frameFrustum.FromMatrix(projMatrix * viewMatrix);

	root->Update(dt);
}

void SurfaceRenderer::RenderScene() {
	BuildNodeLists(root);
	SortNodeLists();

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	DrawSkybox();
	//DrawShadowScene();

	DrawNodes();
	DrawHeightmap();
	DrawPointLights();
	CombineBuffers();
	DrawPostProcess();
	PresentScene();
	ClearNodeLists();
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

void SurfaceRenderer::SortNodeLists() {
	std::sort(transparentNodeList.rbegin(), // note the r!
		transparentNodeList.rend(), // note the r!
		SceneNode::CompareByCameraDistance);

	std::sort(nodeList.begin(),
		nodeList.end(),
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

void SurfaceRenderer::DrawHeightmap(){

	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);
	//glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	BindShader(sceneShader);
	viewMatrix = camera->BuildViewMatrix();
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
	/*
	glUniform1i(glGetUniformLocation(sceneShader->GetProgram(),
		"shadowTex"), 2);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	*/
	glUniform3fv(glGetUniformLocation(sceneShader->GetProgram(),
		"cameraPos"), 1, (float*)&camera->GetPosition());

	modelMatrix.ToIdentity();
	textureMatrix.ToIdentity();

	UpdateShaderMatrices();
	SetShaderLight(*light);

	heightMap->Draw();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SurfaceRenderer::DrawNodes() {

	
	for (const auto& i : nodeList) {
		if (i->GetMeshAnimation() != NULL)
			DrawAnimatedNode(i);
		else
			DrawNode(i);
	}

	for (const auto& i : transparentNodeList) {
		DrawNode(i);
	}
}

void SurfaceRenderer::DrawNode(SceneNode* n) {

	BindShader(sceneShader);
	UpdateShaderMatrices();
	glBindFramebuffer(GL_FRAMEBUFFER, bufferFBO);

	if (n->GetMesh()) {
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

	}

	for (vector < SceneNode* >::const_iterator
		i = n->GetChildIteratorStart();
		i != n->GetChildIteratorEnd(); ++i) {
		DrawNode(*i);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SurfaceRenderer::DrawNodeShadows() {

	//BindShader(sceneShader);
	//UpdateShaderMatrices();

	for (const auto& i : nodeList) {
		DrawNodeShadow(i);
	}

	/*
	BindShader(skinningShader);
	glUniform1i(glGetUniformLocation(skinningShader->GetProgram(),
		"diffuseTex"), 0);

	UpdateShaderMatrices();

	for (const auto& i : animatedNodeList) {
		DrawAnimatedNode(i);
	}
	*/
	//BindShader(sceneShader);
	for (const auto& i : transparentNodeList) {
		DrawNodeShadow(i);
	}
}

void SurfaceRenderer::DrawNodeShadow(SceneNode* n) {
	if (n->GetMesh()) {
		Matrix4 model = n->GetWorldTransform() *
			Matrix4::Scale(n->GetModelScale());
		/*
		glUniformMatrix4fv(
			glGetUniformLocation(sceneShader->GetProgram(),
				"modelMatrix"), 1, false, model.values);

		for (int i = 0; i < n->GetMesh()->GetSubMeshCount(); ++i) {
			//modelMatrix = model;
			//UpdateShaderMatrices();
			n->GetMesh()->DrawSubMesh(i);
		}*/	
		modelMatrix = model;
		UpdateShaderMatrices();
		n->GetMesh()->Draw();
	}

	for (vector < SceneNode* >::const_iterator
		i = n->GetChildIteratorStart();
		i != n->GetChildIteratorEnd(); ++i) {
		DrawNode(*i);
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

void SurfaceRenderer::DrawShadowScene() {
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);

	glClear(GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, SHADOWSIZE, SHADOWSIZE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	BindShader(shadowShader);

	viewMatrix = Matrix4::BuildViewMatrix(
		light->GetPosition(), Vector3(0, 0, 0));
	projMatrix = Matrix4::Perspective(1, 100, 1, 45);
	shadowMatrix = projMatrix * viewMatrix; // used later

	heightMap->Draw();
	//DrawNodeShadows();

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glViewport(0, 0, width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
	for (int i = 0; i < LIGHT_NUM; ++i) {
		Light& l = pointLights[i];
		SetShaderLight(l);
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

void SurfaceRenderer::CombineBuffers() {
	glBindFramebuffer(GL_FRAMEBUFFER, processFBO);
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
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	BindShader(sceneShader);
	modelMatrix.ToIdentity();
	viewMatrix.ToIdentity();
	projMatrix.ToIdentity();
	UpdateShaderMatrices();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, bufferColourTex[0]);
	glUniform1i(glGetUniformLocation(
		sceneShader->GetProgram(), "diffuseTex"), 0);
	quad->Draw();
}