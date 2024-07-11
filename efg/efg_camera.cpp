#include "efg_camera.h"
#include <Windows.h>

// cam speed
const float rotationSpeed = 0.01f;
const float zoomSpeed = 0.1f;

// mouse
bool rmbDown = false;
int prevMouseX, prevMouseY;

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
    // Update camera history
    camera.preView = camera.view;
    camera.prevProj = camera.proj;

    POINT cursorPos;
    GetCursorPos(&cursorPos);
    ScreenToClient(window, &cursorPos);

    int mouseX = cursorPos.x - 1920 / 2;
    int mouseY = cursorPos.y - 1080 / 2;

    if (rmbDown)
    {
        // Calculate the rotation angles based on mouse movement
        float yaw = rotationSpeed * static_cast<float>(mouseX - prevMouseX);
        float pitch = rotationSpeed * static_cast<float>(mouseY - prevMouseY);

        float smoothFactor = 0.2f; // Adjust this value for smoother movement
        yaw = smoothFactor * yaw + (1.0f - smoothFactor) * camera.prevYaw;
        pitch = smoothFactor * pitch + (1.0f - smoothFactor) * camera.prevPitch;

        // Create quaternions for the yaw and pitch rotations
        XMVECTOR up = XMLoadFloat3(&camera.up);
        XMVECTOR eye = XMLoadFloat3(&camera.eye);
        XMVECTOR center = XMLoadFloat3(&camera.center);

        XMVECTOR direction = XMVectorSubtract(center, eye);
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, direction));

        XMVECTOR yawQuat = XMQuaternionRotationAxis(up, yaw);
        XMVECTOR pitchQuat = XMQuaternionRotationAxis(right, pitch);

        // Apply the rotations to the direction vector
        direction = XMVector3Rotate(direction, yawQuat);
        direction = XMVector3Rotate(direction, pitchQuat);

        // Update the camera's center position
        center = XMVectorAdd(eye, direction);
        XMStoreFloat3(&camera.center, center);

        camera.prevYaw = yaw;
        camera.prevPitch = pitch;
    }

    prevMouseX = mouseX;
    prevMouseY = mouseY;

    // Handle camera translation based on keyboard input
    XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&camera.center), XMLoadFloat3(&camera.eye)));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, XMLoadFloat3(&camera.up)));
    XMVECTOR eye = XMLoadFloat3(&camera.eye);
    XMVECTOR center = XMLoadFloat3(&camera.center);
    if (GetAsyncKeyState('W') & 0x8000)
    {
        eye = XMVectorAdd(eye, XMVectorScale(forward, zoomSpeed));
        center = XMVectorAdd(center, XMVectorScale(forward, zoomSpeed));
    }
    if (GetAsyncKeyState('S') & 0x8000)
    {
        eye = XMVectorSubtract(eye, XMVectorScale(forward, zoomSpeed));
        center = XMVectorSubtract(center, XMVectorScale(forward, zoomSpeed));
    }
    if (GetAsyncKeyState('A') & 0x8000)
    {
        eye = XMVectorAdd(eye, XMVectorScale(right, zoomSpeed));
        center = XMVectorAdd(center, XMVectorScale(right, zoomSpeed));
    }
    if (GetAsyncKeyState('D') & 0x8000)
    {
        eye = XMVectorSubtract(eye, XMVectorScale(right, zoomSpeed));
        center = XMVectorSubtract(center, XMVectorScale(right, zoomSpeed));
    }

    XMStoreFloat3(&camera.eye, eye);
    XMStoreFloat3(&camera.center, center);

    // Handle mouse button states
    rmbDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    // Update view matrix
    XMMATRIX viewMatrix = XMMatrixLookAtLH(eye, center, XMLoadFloat3(&camera.up));
    XMStoreFloat4x4(&camera.view, viewMatrix);

    // Update projection matrix
    float const aspect_ratio = 1920 / 1080.0f;
    XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(0.6f, aspect_ratio, 0.1f, 10000.0f);
    XMStoreFloat4x4(&camera.proj, projMatrix);

    // Update view-projection matrix
    XMMATRIX viewProjMatrix = XMMatrixMultiply(viewMatrix, projMatrix);
    XMStoreFloat4x4(&camera.viewProj, viewProjMatrix);

    // Update previous view-projection matrix
    XMMATRIX prevViewMatrix = XMLoadFloat4x4(&camera.preView);
    XMMATRIX prevProjMatrix = XMLoadFloat4x4(&camera.prevProj);
    XMMATRIX prevViewProjMatrix = XMMatrixMultiply(prevViewMatrix, prevProjMatrix);
    XMStoreFloat4x4(&camera.prevViewProj, prevViewProjMatrix);
}
