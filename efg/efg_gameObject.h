#pragma once
#include <DirectXMath.h>
#include "efg_resources.h"

using namespace DirectX;

struct ObjectConstants
{
	bool isInstanced;
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
	EfgVertexBuffer vertexBuffer;
	EfgIndexBuffer indexBuffer;
};

class InstanceableObject : public GameObject
{
	InstanceableObject() { constants.isInstanced = true; }
	virtual void Render() override;
	void AddInstance(Transform instanceTransform);

	std::vector<Transform> instances;
};
