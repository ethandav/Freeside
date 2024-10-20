#include "efg_gameObject.h"

XMMATRIX Transform::GetTransformMatrix() const
{
    XMVECTOR rotationRadians = XMVectorSet(XMConvertToRadians(rotation.x), XMConvertToRadians(rotation.y), XMConvertToRadians(rotation.z), 0.0f);
    XMMATRIX translationMatrix = XMMatrixTranslation(translation.x, translation.y, translation.z);
    XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(rotationRadians.m128_f32[0], rotationRadians.m128_f32[1], rotationRadians.m128_f32[2]);
    XMMATRIX scalingMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX transformMatrix = scalingMatrix * rotationMatrix * translationMatrix;

    return transformMatrix;
}

void GameObject::Update(float deltaTime)
{

}

void GameObject::Render()
{
    
}

void InstanceableObject::Render()
{

}

void InstanceableObject::AddInstance(Transform instanceTransform)
{
    instances.push_back(instanceTransform);
}
