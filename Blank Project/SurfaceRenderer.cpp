#include "SurfaceRenderer.h"
#include "../nclgl/Light.h"
#include "../nclgl/AutomaticCamera.h"
#include "../nclgl/HeightMap.h"
#include "../nclgl/MeshMaterial.h"
#include "../nclgl/MeshAnimation.h"
#include <algorithm>

SurfaceRenderer::SurfaceRenderer(Window& parent) : OGLRenderer(parent) {
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

	shader = new Shader("BumpVertex.glsl", "BumpFragment.glsl");

	skyboxShader = new Shader(
		"skyboxVertex.glsl", "skyboxFragment.glsl");

	skinningShader = new Shader("SkinningVertex.glsl", "texturedFragment.glsl");

	if (!shader->LoadSuccess() ||
		!skyboxShader->LoadSuccess()){
		return;
	}

	Vector3 heightmapSize = heightMap->GetHeightmapSize();

	camera = new AutomaticCamera(-45.0f, 0.0f,
		heightmapSize * Vector3(0.5f, 5.0f, 0.5f),false);

	light = new Light(heightmapSize * Vector3(0.5f, 1.5f, 0.5f),
		Vector4(1, 1, 1, 1), heightmapSize.x * 0.5f);

	projMatrix = Matrix4::Perspective(1.0f, 15000.0f,
		(float)width / (float)height, 45.0f);

	AddMeshesToScene();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	currentFrame = 0;
	frameTime = 0.0f;

	init = true;
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
		s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 0.5f));
		s->SetTransform(Matrix4::Translation(
			Vector3(4296.0, 900.0, 6096.0 -300.0f + 100.0f + 100 * i)));
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

		root->AddChild(s);
	}

	//Adding Robot Guard
	
	mesh = Mesh::LoadFromMeshFile("Role_T.msh");
	anim = new MeshAnimation("Role_T.anm");
	material = new MeshMaterial("Role_T.mat");
	SceneNode* s = new SceneNode();

	s->SetColour(Vector4(1.0f, 1.0f, 1.0f, 0.5f));
	s->SetTransform(Matrix4::Translation(
		Vector3(4296.0,500.0, 5096.0)) * Matrix4::Rotation(180,Vector3(0,1,0)));
	s->SetModelScale(Vector3(300.0f, 300.0f, 300.0f));
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

	currentFrame = 0;
	frameTime = 0.0f;
}

SurfaceRenderer ::~SurfaceRenderer(void) {
	delete camera;
	delete heightMap;
	delete shader;
	delete light;
	delete root;
	delete skyboxShader;
	delete quad;
	delete anim;
	delete mesh;
	glDeleteTextures(1, &groundText);
}

void SurfaceRenderer::UpdateScene(float dt) {
	camera->UpdateCamera(dt);

	viewMatrix = camera->BuildViewMatrix();
	frameFrustum.FromMatrix(projMatrix * viewMatrix);

	root->Update(dt);
	
	frameTime -= dt;
	while (frameTime < 0.0f) {
		currentFrame = (currentFrame + 1) % anim->GetFrameCount();
		frameTime += 1.0f / anim->GetFrameRate();
	}
	
}

void SurfaceRenderer::RenderScene() {
	BuildNodeLists(root);
	SortNodeLists();

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	DrawSkybox();
	DrawHeightmap();

	DrawNodes();
	ClearNodeLists();
}

void SurfaceRenderer::DrawHeightmap(){
	BindShader(shader);

	glUniform1i(glGetUniformLocation(
		shader->GetProgram(), "diffuseTex"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, groundText);

	glUniform1i(glGetUniformLocation(
		shader->GetProgram(), "bumpTex"), 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, bumpmap);

	glUniform3fv(glGetUniformLocation(shader->GetProgram(),
		"cameraPos"), 1, (float*)&camera->GetPosition());

	modelMatrix.ToIdentity();
	textureMatrix.ToIdentity();

	UpdateShaderMatrices();
	SetShaderLight(*light);

	heightMap->Draw();
}

void SurfaceRenderer::BuildNodeLists(SceneNode* from) {
	if (frameFrustum.InsideFrustum(*from)) {
		Vector3 dir = from->GetWorldTransform().GetPositionVector() -
			camera->GetPosition();
		from->SetCameraDistance(Vector3::Dot(dir, dir));

		if (from->GetMeshAnimation() != NULL)
		{
			animatedNodeList.push_back(from);
		}else if (from->GetColour().w < 1.0f) {
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

	std::sort(animatedNodeList.begin(),
		animatedNodeList.end(),
		SceneNode::CompareByCameraDistance);

	std::sort(nodeList.begin(),
		nodeList.end(),
		SceneNode::CompareByCameraDistance);
}

void SurfaceRenderer::DrawNodes() {

	BindShader(shader);

	for (const auto& i : nodeList) {
		DrawNode(i);
	}



	for (const auto& i : animatedNodeList) {
		DrawAnimatedNode(i);
	}

	BindShader(shader);

	for (const auto& i : transparentNodeList) {
		DrawNode(i);
	}
}

void SurfaceRenderer::DrawNode(SceneNode* n) {
	if (n->GetMesh()) {
		Matrix4 model = n->GetWorldTransform() *
			Matrix4::Scale(n->GetModelScale());

		glUniformMatrix4fv(
			glGetUniformLocation(shader->GetProgram(),
				"modelMatrix"), 1, false, model.values);

		glUniform4fv(glGetUniformLocation(shader->GetProgram(),
			"nodeColour"), 1, (float*)&n->GetColour());

		for (int i = 0; i < n->GetMesh()->GetSubMeshCount(); ++i) {
			texture = n->GetTexture(i);
			
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);

			glUniform1i(glGetUniformLocation(shader->GetProgram(),
				"useTexture"), texture);

			n->GetMesh()->DrawSubMesh(i);
		}
		
	}

	for (vector < SceneNode* >::const_iterator
		i = n->GetChildIteratorStart();
		i != n->GetChildIteratorEnd(); ++i) {
		DrawNode(*i);
	}
}

void SurfaceRenderer::DrawAnimatedNode(SceneNode* n)
{
	//glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

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
	const Matrix4* frameData = n->GetMeshAnimation()
								->GetJointData(currentFrame);

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

}

void SurfaceRenderer::DrawSkybox() {
	glDepthMask(GL_FALSE);

	BindShader(skyboxShader);
	UpdateShaderMatrices();

	quad->Draw();

	glDepthMask(GL_TRUE);
}

void SurfaceRenderer::ClearNodeLists() {
	transparentNodeList.clear();
	animatedNodeList.clear();
	nodeList.clear();
}