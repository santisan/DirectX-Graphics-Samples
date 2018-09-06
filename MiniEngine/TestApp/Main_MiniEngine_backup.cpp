#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "GameInput.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "BufferManager.h"
#include "Camera.h"
#include "CameraController.h"
#include "AssimpModelLoader.h"
#include "H3DModelLoader.h"
#include "SkinnedModel.h"

#include "CompiledShaders/TestAppVS.h"
#include "CompiledShaders/TestAppPS.h"

using namespace GameCore;
using namespace Graphics;
using namespace Math;

class TestApp : public GameCore::IGameApp
{
public:

    TestApp() {}

    virtual void Startup() override;
    virtual void Cleanup() override;

    virtual void Update(float deltaTime) override;
    virtual void RenderScene() override;

private:
	std::unique_ptr<Model> mpModel;
	GraphicsPSO mPipelineState;
	RootSignature mRootSignature;
	Camera mCamera;
	std::unique_ptr<CameraController> mpCameraController;
	Matrix4 mViewProjection;
	D3D12_VIEWPORT mMainViewport;
	D3D12_RECT mMainScissor;
};

CREATE_APPLICATION(TestApp)

void TestApp::Startup()
{
	mRootSignature.Reset(1, 0);
	//mRootSignature.InitStaticSampler(0, SamplerLinearWrapDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	mRootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
	//mRootSignature[1].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL);
	//mRootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, D3D12_SHADER_VISIBILITY_PIXEL);
	//mRootSignature[3].InitAsConstants(1, 2, D3D12_SHADER_VISIBILITY_VERTEX);
	mRootSignature.Finalize(L"TestApp", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	const uint32_t kNumInputLayouts = 1;
	D3D12_INPUT_ELEMENT_DESC inputLayout[kNumInputLayouts] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }/*,
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }*/
		// TODO: skinning layout
	};

	mPipelineState.SetRootSignature(mRootSignature);
	mPipelineState.SetRasterizerState(RasterizerDefault);
	mPipelineState.SetInputLayout(kNumInputLayouts, inputLayout);
	mPipelineState.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mPipelineState.SetBlendState(BlendDisable);
	mPipelineState.SetDepthStencilState(DepthStateReadWrite);
	mPipelineState.SetRenderTargetFormats(1, &g_SceneColorBuffer.GetFormat(), g_SceneDepthBuffer.GetFormat());
	mPipelineState.SetVertexShader(g_pTestAppVS, sizeof(g_pTestAppVS));
	mPipelineState.SetPixelShader(g_pTestAppPS, sizeof(g_pTestAppPS));
	mPipelineState.Finalize();

	g_SceneColorBuffer.SetClearColor(Color{0.8f, 0.1f, 0.1f});

	TextureManager::Initialize(L"Textures/");

	AssimpModelLoader assimpLoader;
	H3DModelLoader h3dLoader;
	//mpModel = h3dLoader.LoadModel("Models/sponza.h3d");
	//mpModel = assimpLoader.LoadSkinnedModel("Models/Running.fbx");
	mpModel = assimpLoader.LoadModel("Models/duck.dae");
	ASSERT(mpModel, "Failed to load model");
	ASSERT(mpModel->m_Header.meshCount > 0, "Model contains no meshes");

	const float modelRadius = Length(mpModel->m_Header.boundingBox.max - mpModel->m_Header.boundingBox.min) * 0.5f;
	const Vector3 eye = Vector3{kZero}; //(mpModel->m_Header.boundingBox.min + mpModel->m_Header.boundingBox.max) * 0.5f + Vector3(modelRadius * 0.5f, 0.0f, 0.0f);
	mCamera.SetEyeAtUp(eye, Vector3(kZero), Vector3(kYUnitVector));
	mCamera.SetZRange(1.0f, 10000.0f);
	mpCameraController = std::make_unique<CameraController>(mCamera, Vector3(kYUnitVector));
}

void TestApp::Cleanup()
{
	mpModel->Clear();
}

void TestApp::Update(float deltaTime)
{
    ScopedTimer _prof(L"Update State");

	mpCameraController->Update(deltaTime);
	mViewProjection = mCamera.GetViewProjMatrix();

	mMainViewport.Width = static_cast<float>(g_SceneColorBuffer.GetWidth());
	mMainViewport.Height = static_cast<float>(g_SceneColorBuffer.GetHeight());
	mMainViewport.MinDepth = 0.0f;
	mMainViewport.MaxDepth = 1.0f;

	mMainScissor.left = 0;
	mMainScissor.top = 0;
	mMainScissor.right = static_cast<LONG>(g_SceneColorBuffer.GetWidth());
	mMainScissor.bottom = static_cast<LONG>(g_SceneColorBuffer.GetHeight());
}

void TestApp::RenderScene()
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    gfxContext.ClearColor(g_SceneColorBuffer);
    gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV());
    gfxContext.SetViewportAndScissor(mMainViewport, mMainScissor);

	gfxContext.SetRootSignature(mRootSignature);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetIndexBuffer(mpModel->m_IndexBuffer.IndexBufferView());
	gfxContext.SetVertexBuffer(0, mpModel->m_VertexBuffer.VertexBufferView());
	gfxContext.SetPipelineState(mPipelineState);

	// Render model
	struct VSConstants {
		Matrix4 worldViewProjection;
	} vsConstants;
	vsConstants.worldViewProjection = mViewProjection;

	gfxContext.SetDynamicConstantBufferView(0, sizeof(vsConstants), &vsConstants);

	//uint32_t materialIdx = 0xFFFFFFFFul;
	const uint32_t VertexStride = mpModel->m_VertexStride;

	for (uint32_t meshIndex = 0; meshIndex < mpModel->m_Header.meshCount; meshIndex++)
	{
		const Mesh& mesh = mpModel->m_pMesh[meshIndex];
		const uint32_t indexCount = mesh.indexCount;
		const uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
		const uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;

		/*if (mesh.materialIndex != materialIdx)
		{
			materialIdx = mesh.materialIndex;
			gfxContext.SetDynamicDescriptors(2, 0, 6, mpModel->GetSRVs(materialIdx));
		}*/

		//gfxContext.SetConstants(4, baseVertex, materialIdx);
		gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);
	}

    gfxContext.Finish();
}
