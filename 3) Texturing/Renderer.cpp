#include "Renderer.h"

Renderer::Renderer(Window& parent) : OGLRenderer(parent) {
	triangle = Mesh::GenerateTriangle();

	texture1 = SOIL_load_OGL_texture(TEXTUREDIR"brick.tga",
		SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, 0);

	texture2 = SOIL_load_OGL_texture(TEXTUREDIR"stainedglass.tga",
		SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, 0);

	if (!texture1 || !texture2) {
		return;
	}


	shader = new Shader("basicTexturedVertex.glsl", "ColourAndTexturedFragment.glsl");
	
		if (!shader -> LoadSuccess()) {
		return;
		
	}
	filtering = true;
	repeating = false;
	init = true;
}

Renderer ::~Renderer(void) {
	delete triangle;
	delete shader;
	glDeleteTextures(1, &texture1);
	glDeleteTextures(1, &texture2);
}

void Renderer::RenderScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	BindShader(shader);
	UpdateShaderMatrices();
	
		glUniform1i(glGetUniformLocation(shader -> GetProgram(),
			"diffuseTex"), 0); // this last parameter
	glActiveTexture(GL_TEXTURE0); // should match this number !
	glBindTexture(GL_TEXTURE_2D, texture1);

	glUniform1i(glGetUniformLocation(shader->GetProgram(),
		"diffuseTex"), 1);
	glActiveTexture(GL_TEXTURE1); // should match this number !
	glBindTexture(GL_TEXTURE_2D, texture2);

		triangle -> Draw();
}

void Renderer::UpdateTextureMatrix(float value) {
	Matrix4 push = Matrix4::Translation(Vector3(-0.5f, -0.5f, 0));
	Matrix4 pop = Matrix4::Translation(Vector3(0.5f, 0.5f, 0));
	Matrix4 rotation = Matrix4::Rotation(value, Vector3(0, 0, 1));
	textureMatrix = pop * rotation * push;
}

void Renderer::ToggleRepeating() {
	repeating = !repeating;
	SetTextureRepeating(texture1, repeating);
	SetTextureRepeating(texture2, repeating);
}


void Renderer::ToggleFiltering() {
	filtering = !filtering;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, texture2);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		filtering ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		filtering ? GL_LINEAR : GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
}