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
// Author:  Alex Nankervis
//

#include "H3DModelLoader.h"
#include "Model.h"
#include "Utility.h"
#include "TextureManager.h"
#include "GraphicsCore.h"
#include "DescriptorHeap.h"
#include "CommandContext.h"
#include <stdio.h>

std::unique_ptr<Model> H3DModelLoader::LoadModel(const char *filename)
{
    FILE *file = nullptr;
	if (0 != fopen_s(&file, filename, "rb")) {
		return nullptr;
	}
    bool ok = false;
	auto model = std::make_unique<Model>();

    if (1 != fread(&model->m_Header, sizeof(Model::Header), 1, file)) goto h3d_load_fail;

	model->m_pMesh = new Mesh [model->m_Header.meshCount];
	model->m_pMaterial = new Material [model->m_Header.materialCount];

    if (model->m_Header.meshCount > 0)
        if (1 != fread(model->m_pMesh, sizeof(Mesh) * model->m_Header.meshCount, 1, file)) goto h3d_load_fail;
    if (model->m_Header.materialCount > 0)
        if (1 != fread(model->m_pMaterial, sizeof(Material) * model->m_Header.materialCount, 1, file)) goto h3d_load_fail;

	model->m_VertexStride = model->m_pMesh[0].vertexStride;
	model->m_VertexStrideDepth = model->m_pMesh[0].vertexStrideDepth;
#if _DEBUG
    for (uint32_t meshIndex = 1; meshIndex < model->m_Header.meshCount; ++meshIndex)
    {
        const Mesh& mesh = model->m_pMesh[meshIndex];
        ASSERT(mesh.vertexStride == model->m_VertexStride);
        ASSERT(mesh.vertexStrideDepth == model->m_VertexStrideDepth);
    }
    for (uint32_t meshIndex = 0; meshIndex < model->m_Header.meshCount; ++meshIndex)
    {
        const Mesh& mesh = model->m_pMesh[meshIndex];

        ASSERT( mesh.attribsEnabled ==
            (attrib_mask_position | attrib_mask_texcoord0 | attrib_mask_normal | attrib_mask_tangent | attrib_mask_bitangent) );
        ASSERT(mesh.attrib[0].components == 3 && mesh.attrib[0].format == attrib_format_float); // position
        ASSERT(mesh.attrib[1].components == 2 && mesh.attrib[1].format == attrib_format_float); // texcoord0
        ASSERT(mesh.attrib[2].components == 3 && mesh.attrib[2].format == attrib_format_float); // normal
        ASSERT(mesh.attrib[3].components == 3 && mesh.attrib[3].format == attrib_format_float); // tangent
        ASSERT(mesh.attrib[4].components == 3 && mesh.attrib[4].format == attrib_format_float); // bitangent

        ASSERT( mesh.attribsEnabledDepth ==
            (attrib_mask_position) );
        ASSERT(mesh.attrib[0].components == 3 && mesh.attrib[0].format == attrib_format_float); // position
    }
#endif

	model->m_pVertexData = new unsigned char[model->m_Header.vertexDataByteSize];
	model->m_pIndexData = new unsigned char[model->m_Header.indexDataByteSize];
	model->m_pVertexDataDepth = new unsigned char[model->m_Header.vertexDataByteSizeDepth];
	model->m_pIndexDataDepth = new unsigned char[model->m_Header.indexDataByteSize];

    if (model->m_Header.vertexDataByteSize > 0)
        if (1 != fread(model->m_pVertexData, model->m_Header.vertexDataByteSize, 1, file)) goto h3d_load_fail;
    if (model->m_Header.indexDataByteSize > 0)
        if (1 != fread(model->m_pIndexData, model->m_Header.indexDataByteSize, 1, file)) goto h3d_load_fail;

    if (model->m_Header.vertexDataByteSizeDepth > 0)
        if (1 != fread(model->m_pVertexDataDepth, model->m_Header.vertexDataByteSizeDepth, 1, file)) goto h3d_load_fail;
    if (model->m_Header.indexDataByteSize > 0)
        if (1 != fread(model->m_pIndexDataDepth, model->m_Header.indexDataByteSize, 1, file)) goto h3d_load_fail;

	model->m_VertexBuffer.Create(L"VertexBuffer", model->m_Header.vertexDataByteSize / model->m_VertexStride, model->m_VertexStride, model->m_pVertexData);
	model->m_IndexBuffer.Create(L"IndexBuffer", model->m_Header.indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), model->m_pIndexData);
    delete [] model->m_pVertexData;
	model->m_pVertexData = nullptr;
    delete [] model->m_pIndexData;
	model->m_pIndexData = nullptr;

	model->m_VertexBufferDepth.Create(L"VertexBufferDepth", model->m_Header.vertexDataByteSizeDepth / model->m_VertexStrideDepth, model->m_VertexStrideDepth, 
		model->m_pVertexDataDepth);
	model->m_IndexBufferDepth.Create(L"IndexBufferDepth", model->m_Header.indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), model->m_pIndexDataDepth);
    delete [] model->m_pVertexDataDepth;
	model->m_pVertexDataDepth = nullptr;
    delete [] model->m_pIndexDataDepth;
	model->m_pIndexDataDepth = nullptr;

    LoadTextures(model.get());

    ok = true;

h3d_load_fail:
	if (EOF == fclose(file)) {
		ok = false;
	}

	if (!ok) return nullptr;
    return std::move(model);
}

bool H3DModelLoader::Save(Model* model, const char *filename) const
{
	ASSERT(model);
    FILE *file = nullptr;
	if (0 != fopen_s(&file, filename, "wb")) {
		return false;
	}
    bool ok = false;

    if (1 != fwrite(&model->m_Header, sizeof(Model::Header), 1, file)) goto h3d_save_fail;

    if (model->m_Header.meshCount > 0)
        if (1 != fwrite(model->m_pMesh, sizeof(Mesh) * model->m_Header.meshCount, 1, file)) goto h3d_save_fail;
    if (model->m_Header.materialCount > 0)
        if (1 != fwrite(model->m_pMaterial, sizeof(Material) * model->m_Header.materialCount, 1, file)) goto h3d_save_fail;

    if (model->m_Header.vertexDataByteSize > 0)
        if (1 != fwrite(model->m_pVertexData, model->m_Header.vertexDataByteSize, 1, file)) goto h3d_save_fail;
    if (model->m_Header.indexDataByteSize > 0)
        if (1 != fwrite(model->m_pIndexData, model->m_Header.indexDataByteSize, 1, file)) goto h3d_save_fail;

    if (model->m_Header.vertexDataByteSizeDepth > 0)
        if (1 != fwrite(model->m_pVertexDataDepth, model->m_Header.vertexDataByteSizeDepth, 1, file)) goto h3d_save_fail;
    if (model->m_Header.indexDataByteSize > 0)
        if (1 != fwrite(model->m_pIndexDataDepth, model->m_Header.indexDataByteSize, 1, file)) goto h3d_save_fail;

    ok = true;

h3d_save_fail:

    if (EOF == fclose(file))
        ok = false;

    return ok;
}

void H3DModelLoader::LoadTextures(Model* model)
{
	ASSERT(model);
    model->m_SRVs = new D3D12_CPU_DESCRIPTOR_HANDLE[model->m_Header.materialCount * 6];

    const ManagedTexture* MatTextures[6] = {};

    for (uint32_t materialIdx = 0; materialIdx < model->m_Header.materialCount; ++materialIdx)
    {
        const Material& pMaterial = model->m_pMaterial[materialIdx];

        // Load diffuse
        MatTextures[0] = TextureManager::LoadFromFile(pMaterial.texDiffusePath, true);
        if (!MatTextures[0]->IsValid())
            MatTextures[0] = TextureManager::LoadFromFile("default", true);

        // Load specular
        MatTextures[1] = TextureManager::LoadFromFile(pMaterial.texSpecularPath, true);
        if (!MatTextures[1]->IsValid())
        {
            MatTextures[1] = TextureManager::LoadFromFile(std::string(pMaterial.texDiffusePath) + "_specular", true);
            if (!MatTextures[1]->IsValid())
                MatTextures[1] = TextureManager::LoadFromFile("default_specular", true);
        }

        // Load emissive
        //MatTextures[2] = TextureManager::LoadFromFile(pMaterial.texEmissivePath, true);

        // Load normal
        MatTextures[3] = TextureManager::LoadFromFile(pMaterial.texNormalPath, false);
        if (!MatTextures[3]->IsValid())
        {
            MatTextures[3] = TextureManager::LoadFromFile(std::string(pMaterial.texDiffusePath) + "_normal", false);
            if (!MatTextures[3]->IsValid())
                MatTextures[3] = TextureManager::LoadFromFile("default_normal", false);
        }

        // Load lightmap
        //MatTextures[4] = TextureManager::LoadFromFile(pMaterial.texLightmapPath, true);

        // Load reflection
        //MatTextures[5] = TextureManager::LoadFromFile(pMaterial.texReflectionPath, true);

        model->m_SRVs[materialIdx * 6 + 0] = MatTextures[0]->GetSRV();
        model->m_SRVs[materialIdx * 6 + 1] = MatTextures[1]->GetSRV();
        model->m_SRVs[materialIdx * 6 + 2] = MatTextures[0]->GetSRV();
        model->m_SRVs[materialIdx * 6 + 3] = MatTextures[3]->GetSRV();
        model->m_SRVs[materialIdx * 6 + 4] = MatTextures[0]->GetSRV();
        model->m_SRVs[materialIdx * 6 + 5] = MatTextures[0]->GetSRV();
    }
}

std::unique_ptr<SkinnedModel> H3DModelLoader::LoadSkinnedModel(const char* filename)
{
	return nullptr;
}
