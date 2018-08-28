#pragma once
#include "ModelLoader.h"

class H3DModelLoader : public  IModelLoader
{
public:
	std::unique_ptr<Model> Load(const char* filename) override;
	
	bool Save(Model* model, const char *filename) const;

private:
	void LoadTextures(Model* model);
};
