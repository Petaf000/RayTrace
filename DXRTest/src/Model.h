#pragma once
#include "Renderer.h"

struct Mesh {
	Vertex2* VertexArray;
	uint32_t VertexNum;

	uint32_t* IndexArray;
	uint32_t IndexNum;

	uint32_t MaterialIndex;
};

struct ModelData {
	std::vector<Mesh> Meshes;

};

class aiNode;
class aiScene;
class Model {
public:
	Model();
	~Model();

	void LoadModel(const std::string& fileName);

private:
	void LoadGLB(ModelData& model);

	void RecursiveNode(aiNode* node, const aiScene* scene, ModelData& model);


	std::string		m_fileName;
	bool			m_loaded = false;

	ComPtr<ID3D12Resource>	m_vertexBuffer;
	ComPtr<ID3D12Resource>	m_indexBuffer;
	unsigned int	m_vertexNum{};
	unsigned int	m_indexNum{};

	float m_maxPosition{};
};

Model::Model() {
}

Model::~Model() {
}