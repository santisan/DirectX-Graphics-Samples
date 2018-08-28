//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//

#pragma once

#include "ModelLoader.h"

class AssimpModelLoader : public IModelLoader
{
public:
	std::unique_ptr<Model> Load(const char* filename) override;

private:
	void Optimize();
	void OptimizeRemoveDuplicateVertices(bool depth);
	void OptimizePostTransform(bool depth);
	void OptimizePreTransform(bool depth);

	Model* m_pCurrentModel = nullptr;
};
