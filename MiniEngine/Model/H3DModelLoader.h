#pragma once
#include "ModelLoader.h"

class H3DModelLoader : public  IModelLoader
{
public:
	std::unique_ptr<Model> LoadModel(const char* filename) override;
	std::unique_ptr<SkinnedModel> LoadSkinnedModel(const char* filename) override;
	
	bool Save(Model* model, const char *filename) const;
};
