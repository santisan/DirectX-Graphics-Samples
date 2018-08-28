#pragma once
#include <memory>

class Model;
class SkinnedModel;

class IModelLoader
{
public:
	virtual std::unique_ptr<Model> LoadModel(const char* filename) = 0;
	virtual std::unique_ptr<SkinnedModel> LoadSkinnedModel(const char* filename) = 0;
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
