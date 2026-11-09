#pragma once
// Carga de modelos .obj con Assimp y de texturas con SOIL2 (igual que en las practicas).

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <cstddef>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "SOIL2/SOIL2.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"

// Cache de texturas compartida entre todos los modelos: cada archivo se carga una sola vez
inline std::map<std::string, GLuint>& TextureCache()
{
	static std::map<std::string, GLuint> cache;
	return cache;
}

inline GLuint TextureFromFile(const char* path, const std::string& directory)
{
	std::string filename = directory + '/' + std::string(path);
	std::map<std::string, GLuint>::iterator it = TextureCache().find(filename);
	if (it != TextureCache().end())
		return it->second;

	GLuint textureID;
	glGenTextures(1, &textureID);
	int width, height;
	unsigned char* image = SOIL_load_image(filename.c_str(), &width, &height, 0, SOIL_LOAD_RGBA);
	if (image == NULL)
	{
		std::cout << "ERROR::TEXTURA no se pudo cargar: " << filename << std::endl;
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
		SOIL_free_image_data(image);
	}
	TextureCache()[filename] = textureID;
	return textureID;
}

class Model
{
public:
	Model(const char* path)
	{
		this->loadModel(path);
	}

	void Draw(Shader shader)
	{
		for (size_t i = 0; i < this->meshes.size(); i++)
			this->meshes[i].Draw(shader);
	}

private:
	std::vector<Mesh> meshes;
	std::string directory;

	void loadModel(std::string path)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
		if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cout << "ERROR::ASSIMP " << importer.GetErrorString() << " (" << path << ")" << std::endl;
			return;
		}
		this->directory = path.substr(0, path.find_last_of('/'));
		this->processNode(scene->mRootNode, scene);
	}

	void processNode(aiNode* node, const aiScene* scene)
	{
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			this->meshes.push_back(this->processMesh(mesh, scene));
		}
		for (unsigned int i = 0; i < node->mNumChildren; i++)
			this->processNode(node->mChildren[i], scene);
	}

	Mesh processMesh(aiMesh* mesh, const aiScene* scene)
	{
		std::vector<Vertex> vertices;
		std::vector<GLuint> indices;
		std::vector<Texture> textures;

		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;
			vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
			if (mesh->mNormals)
				vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			else
				vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
			if (mesh->mTextureCoords[0])
				vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
			else
				vertex.TexCoords = glm::vec2(0.0f, 0.0f);
			vertices.push_back(vertex);
		}

		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			this->loadMaterialTextures(textures, material, aiTextureType_DIFFUSE, "texture_diffuse");
			this->loadMaterialTextures(textures, material, aiTextureType_SPECULAR, "texture_specular");
		}
		return Mesh(vertices, indices, textures);
	}

	void loadMaterialTextures(std::vector<Texture>& out, aiMaterial* mat, aiTextureType type, std::string typeName)
	{
		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
		{
			aiString str;
			mat->GetTexture(type, i, &str);
			Texture texture;
			texture.id = TextureFromFile(str.C_Str(), this->directory);
			texture.type = typeName;
			texture.path = str.C_Str();
			out.push_back(texture);
		}
	}
};
