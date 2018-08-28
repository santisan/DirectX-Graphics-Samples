#pragma once
#include "Model.h"

struct Joint
{
	typedef uint8_t IndexType;
	static const IndexType RootJointIndex = 0xFF;

	Matrix4 inverseBindPose;
	IndexType parentIndex = RootJointIndex;
};

struct Skeleton
{
	std::vector<Joint> joints;
};

struct JointPose
{
	Quaternion rotation;
	Vector3 translation;
	float scale = 1.f;
};

struct SkeletonPose
{
	Skeleton* skeleton = nullptr;
	std::vector<JointPose> localPoses;
	std::vector<Matrix4> globalPoses;
};

struct AnimationSample
{
	std::vector<JointPose> jointPoses;
};

struct AnimationClip
{
	Skeleton* skeleton = nullptr;
	float framesPerSecond = 30.f;
	uint32_t frameCount = 0u;
	std::vector<AnimationSample> samples;
	bool isLooping = false;
};

class SkinnedModel : public Model
{
public:
	SkinnedModel();
	~SkinnedModel();

	Skeleton m_Skeleton;
};

