#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

void Model::LoadModel(const std::string& fileName) {
	m_fileName = fileName;

	ModelData model;
	
	// filenameから拡張子を取得
	std::string ext = fileName.substr(fileName.find_last_of(".") + 1);
	if ( ext == ".glb" ) {
		LoadGLB(model);
	}
	else if ( ext == ".fbx" ) {
		//LoadFBX();
	}
	else if ( ext == ".obj" ) {
		//LoadOBJ();
	}
	else {
		throw std::runtime_error("この3Dオブジェクトはサポートしてません");
	}
}

void Model::LoadGLB(ModelData& model) {

	// DirectX用に変換してGLBファイルを読み込む
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(m_fileName.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);

	if ( !scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode )
		throw std::runtime_error("GLBファイルの読み込みに失敗しました");

	for ( uint16_t i = 0; i < scene->mNumMeshes; i++ ) {
		aiMesh* mesh = scene->mMeshes[i];
		model.VertexNum = mesh->mNumVertices;
		model.IndexNum = mesh->mNumFaces * 3;
		model.VertexArray = new Vertex2[model.VertexNum];
		model.IndexArray = new unsigned int[model.IndexNum];
		for ( uint16_t i = 0; i < model.VertexNum; i++ ) {
			m_maxPosition = max(m_maxPosition, mesh->mVertices[i].x);
			m_maxPosition = max(m_maxPosition, mesh->mVertices[i].y);
			m_maxPosition = max(m_maxPosition, mesh->mVertices[i].z);
		}
	}

	scene->mMeshes[0];


	for ( uint16_t meshNum = 0; meshNum < scene->mNumMeshes; meshNum++ ) {
		aiMesh* mesh = scene->mMeshes[meshNum];
		model.VertexNum = mesh->mNumVertices;
		model.IndexNum = mesh->mNumFaces * 3;
		model.VertexArray = new Vertex2[model.VertexNum];
		model.IndexArray = new unsigned int[model.IndexNum];
		for ( uint16_t i = 0; i < model.VertexNum; i++ ) {
			model.VertexArray[i].position = XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
			model.VertexArray[i].normal = XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			model.VertexArray[i].texcoord = XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}
		for ( uint16_t i = 0; i < model.IndexNum; i++ ) {
			model.IndexArray[i] = mesh->mFaces[i / 3].mIndices[i % 3];
		}





		//テクスチャ読み込み
		for ( int i = 0; i < m_AiScene->mNumTextures; i++ ) {
			aiTexture* aitexture = m_AiScene->mTextures[i];

			ID3D11ShaderResourceView* texture;

			// テクスチャ読み込み
			TexMetadata metadata;
			ScratchImage image;
			LoadFromWICMemory(aitexture->pcData, aitexture->mWidth, WIC_FLAGS_NONE, &metadata, image);
			CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
			assert(texture);

			m_Texture[aitexture->mFilename.data] = texture;
		}
	}
}


void Model::RecursiveNode(aiNode* node, const aiScene* scene, ModelData& model) {

	node->mTransformation;
	//ノードのメッシュを取得
	for ( uint16_t i = 0; i < node->mNumMeshes; i++ ) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		model.VertexNum = mesh->mNumVertices;
		model.IndexNum = mesh->mNumFaces * 3;
		model.VertexArray = new Vertex2[model.VertexNum];
		model.IndexArray = new unsigned int[model.IndexNum];
		for ( uint16_t i = 0; i < model.VertexNum; i++ ) {
			model.VertexArray[i].position = XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
			model.VertexArray[i].normal = XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			model.VertexArray[i].texcoord = XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}
		for ( uint16_t i = 0; i < model.IndexNum; i++ ) {
			model.IndexArray[i] = mesh->mFaces[i / 3].mIndices[i % 3];
		}
	}
	for ( uint16_t i = 0; i < node->mNumChildren; i++ ) {
		RecursiveNode(node->mChildren[i], scene, model);
	}
}
