#pragma once
#include "Model.h"

typedef uint16_t JointIndexType;
static const JointIndexType kRootJointParentIndex = 0xFFFF;

struct Joint
{
	std::string name;
	Matrix4 inverseBindPose = Matrix4{kIdentity};
	JointIndexType parentIndex = kRootJointParentIndex;
};

struct Skeleton
{
	std::vector<Joint> joints;
};

struct JointPose
{
	float scale = 1.f;
	Quaternion rotation = Quaternion{kIdentity};
	Vector3 translation = Vector3{kZero};
};

/*struct SkeletonPose
{
	Skeleton* skeleton = nullptr;
	std::vector<JointPose> localPoses;
	std::vector<Matrix4> globalPoses;
};*/

struct AnimationSample
{
	std::vector<JointPose> jointPoses;
};

struct AnimationClip
{
	std::string name;
	Skeleton* skeleton = nullptr;
	float durationSeconds = 0.f;
	float framesPerSecond = 0.f;
	uint32_t frameCount = 0u;
	std::vector<AnimationSample> samples;
	bool isLooping = false;
};

class SkinnedModel : public Model
{
public:
	SkinnedModel();
	~SkinnedModel();

	// Use IDs and store these in an AnimationManager class
	Skeleton m_Skeleton;
	AnimationClip m_AnimationClip;
};

