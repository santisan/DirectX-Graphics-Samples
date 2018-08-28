#include "ModelLoader.h"
#include "H3DModelLoader.h"
//#include "AssimpModelLoader.h"

std::unique_ptr<IModelLoader> ModelLoaderFactory::CreateModelLoader(EModelLoaderType LoaderType)
{
	switch (LoaderType)
	{
	case EModelLoaderType::H3D: return std::make_unique<H3DModelLoader>();
	//case EModelLoaderType::Assimp: return std::make_unique<AssimpModelLoader>();
	}
	return nullptr;
}
