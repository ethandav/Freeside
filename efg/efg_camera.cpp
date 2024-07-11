#include "efg_camera.h"

Camera efgCreateCamera(EfgContext efg, const XMFLOAT3& eye, const XMFLOAT3& center)
{
	Camera camera = {};

	const float aspectRatio = 1920 / 1080.0f;
	camera.eye = eye;
	camera.center = center;
	camera.up = XMFLOAT3(0.0f, 1.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&center), XMLoadFloat3(&camera.up));
	XMStoreFloat4x4(&camera.view, view);

	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.6f, aspectRatio, 0.1f, 10000.0f);
	XMStoreFloat4x4(&camera.proj, proj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMStoreFloat4x4(&camera.viewProj, viewProj);

	camera.preView = camera.view;
	camera.prevProj = camera.proj;
	camera.prevViewProj = camera.viewProj;

	return camera;
}

void efgUpdateCamera(EfgContext efg, EfgWindow window, Camera& camera)
{

}
