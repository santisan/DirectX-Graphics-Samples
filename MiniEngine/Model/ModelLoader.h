#pragma once
#include <memory>

class Model;

class IModelLoader
{
public:
	virtual std::unique_ptr<Model> Load(const char* filename) = 0;
};

enum class EModelLoaderType
{
	H3D,
	Assimp
};

class ModelLoaderFactory
{
public:
	std::unique_ptr<IModelLoader> CreateModelLoader(EModelLoaderType LoaderType);
};
