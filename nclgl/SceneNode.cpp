#include "SceneNode.h"

SceneNode::SceneNode(Mesh* mesh, Vector4 colour, MeshAnimation* anim) {
	this->mesh = mesh;
	this->anim = anim;
	this->colour = colour;
	parent = NULL;
	modelScale = Vector3(1, 1, 1);
	boundingRadius = 1.0f;
	distanceFromCamera = 0.0f;
	textures.emplace_back(0);
}

SceneNode::~SceneNode(void) {
	for (unsigned int i = 0; i < children.size(); ++i) {
		delete children[i];
	}
}

void SceneNode::AddChild(SceneNode* s) {
	children.push_back(s);
	s -> parent = this;
}

void SceneNode::Draw(const OGLRenderer& r) {
	if (mesh) { mesh->Draw(); };
}

void SceneNode::Update(float dt) {
	if (parent) { // This node has a parent ...
		worldTransform = parent -> worldTransform * transform;
		
	}
	else { // Root node , world transform is local transform !
		worldTransform = transform;
	}
	for (vector < SceneNode* >::iterator i = children.begin();
		i != children.end(); ++i) {
		(*i) -> Update(dt);
	}
}

void SceneNode::AddTexture(GLuint tex) { 
	if (textures[0] != 0)
		textures.emplace_back(tex);
	else
		textures[0] = tex;
};