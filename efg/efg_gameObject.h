#pragma once
#include <DirectXMath.h>
#include "efg_resources.h"

using namespace DirectX;

struct ObjectConstants
{
	uint32_t isInstanced = false;
	uint32_t useTransform = false;
	float padding[2];
};

class Transform
{
public:

	XMMATRIX GetTransformMatrix() const;

	XMFLOAT3 translation;
	XMFLOAT3 rotation;
	XMFLOAT3 scale;
};

class GameObject
{
public:
	virtual void Update(float deltaTime);
	virtual void Render();

	ObjectConstants constants;
	Transform transform;
	EfgMaterialBuffer material;
	EfgBuffer vertexBuffer;
	EfgBuffer indexBuffer;
	EfgBuffer transformBuffer;
	EfgBuffer constantsBuffer;
};

class InstanceableObject : public GameObject
{
public:
	InstanceableObject() { constants.isInstanced = true; }
	virtual void Render() override;
	void AddInstance(Transform instanceTransform);

	std::vector<Transform> instances;
};
