#include "AutomaticCamera.h"
#include "Window.h"

void AutomaticCamera::ResetTrackCamera()
{
	position = cameraStates[0].GetPosition();
	pitch = cameraStates[0].GetPitch();
	yaw = cameraStates[0].GetYaw();

	currentState = 0;
	timeElapsed = 0;
}

void AutomaticCamera::toggleIsFree()
{
	isFree = !isFree;
	if (!isFree)
		AutomaticCamera::ResetTrackCamera();
	else
		baseSpeed = 30.0f;
}

void AutomaticCamera::UpdateCamera(float dt) {
	if (Window::GetKeyboard()->KeyDown(KEYBOARD_1) &&
		!Window::GetKeyboard()->KeyHeld(KEYBOARD_1))
	{
		AutomaticCamera::InstantiateTrack1();
		AutomaticCamera::toggleIsFree();
	}

	if (Window::GetKeyboard()->KeyDown(KEYBOARD_2) &&
		!Window::GetKeyboard()->KeyHeld(KEYBOARD_2))
	{
		AutomaticCamera::InstantiateTrack2();
		AutomaticCamera::toggleIsFree();
	}

	if (isFree)
		AutomaticCamera::UpdateFreeCamera(dt);
	else
		AutomaticCamera::UpdateTrackCamera(dt);
}

void AutomaticCamera::UpdateFreeCamera(float dt) {
	pitch -= (Window::GetMouse()->GetRelativePosition().y);
	yaw -= (Window::GetMouse()->GetRelativePosition().x);

	pitch = std::min(pitch, 90.0f);
	pitch = std::max(pitch, -90.0f);

	if (yaw < 0) {
		yaw += 360.0f;
	}
	if (yaw > 360.0f) {
		yaw -= 360.0f;
	}

	Matrix4 rotation = Matrix4::Rotation(yaw, Vector3(0, 1, 0));

	Vector3 forward = rotation * Vector3(0, 0, -1);
	Vector3 right = rotation * Vector3(1, 0, 0);

	if (Window::GetMouse()->GetWheelMovement() > 0) {
		baseSpeed = baseSpeed * 1.25f;
	}
	if (Window::GetMouse()->GetWheelMovement() < 0) {
		baseSpeed = baseSpeed * 0.25f;
	}
	if (Window::GetMouse()->ButtonDown(MOUSE_MIDDLE)) {
		baseSpeed = 30.0f;
	}

	float speed = baseSpeed * dt;

	if (Window::GetKeyboard()->KeyDown(KEYBOARD_W)) {
		position += forward * speed;
	}
	if (Window::GetKeyboard()->KeyDown(KEYBOARD_S)) {
		position -= forward * speed;
	}
	if (Window::GetKeyboard()->KeyDown(KEYBOARD_A)) {
		position -= right * speed;
	}
	if (Window::GetKeyboard()->KeyDown(KEYBOARD_D)) {
		position += right * speed;
	}

	if (Window::GetKeyboard()->KeyDown(KEYBOARD_SHIFT) || 
		Window::GetKeyboard()->KeyDown(KEYBOARD_C)) {
		position.y -= speed;
	}

	if (Window::GetKeyboard()->KeyDown(KEYBOARD_SPACE)) {
		position.y += speed;
	}
}

void AutomaticCamera::UpdateTrackCamera(float dt)
{
	timeElapsed += dt;

	if (timeElapsed >= stateChangeTimes[currentState])
		if (stateChangeTimes[currentState + 1] != -1)
			currentState++;
		else
		{
			isFree = true;
		}

	float moveSpeed = dt * baseSpeed;
	
	Vector3 targetDirNormalised = (cameraStates[currentState + 1].GetPosition() - position).Normalised();

	position += targetDirNormalised * moveSpeed;

	float targetYaw = (abs(cameraStates[currentState + 1].GetYaw() - yaw) < 180) ?
						cameraStates[currentState + 1].GetYaw() - yaw:
						-(cameraStates[currentState + 1].GetYaw() - yaw);

	float targetPitch = cameraStates[currentState + 1].GetPitch() - pitch;

	float rotSpeed = dt * 10;

	Vector2 targetRotNormalised = Vector2(targetYaw, targetPitch).Normalised();

	yaw += targetRotNormalised.x * rotSpeed;
	pitch += targetRotNormalised.y * rotSpeed;

	pitch = std::min(pitch, 90.0f);
	pitch = std::max(pitch, -90.0f);

	if (yaw < 0) {
		yaw += 360.0f;
	}
	if (yaw > 360.0f) {
		yaw -= 360.0f;
	}
}

void AutomaticCamera::InstantiateTrack1()
{
	currentTrack = 1;
	baseSpeed = 40;
	cameraStates[0] = CameraState(20, 30, Vector3(4296.0, 600.0, 4096.0));
	stateChangeTimes[0] = 3;

	cameraStates[1] = CameraState(-20, 350, Vector3(4296.0, 500.0, 4096.0));
	stateChangeTimes[1] = -1;

	cameraStates[2] = CameraState(20, 0, Vector3(4296.0, 600.0, 4096.0));
	stateChangeTimes[2] = 6;

	cameraStates[3] = CameraState(20, 0, Vector3(4296.0, 600.0, 4096.0));
	stateChangeTimes[3] = 8;

	cameraStates[4] = CameraState(-50, 0, Vector3(4296.0, 600.0, 4096.0));
	stateChangeTimes[4] = -1;
}

void AutomaticCamera::InstantiateTrack2()
{
	currentTrack = 2;

	baseSpeed = 600;
	cameraStates[0] = CameraState(-10, 180, Vector3(8910.0, 1050.0, 4230.0));
	stateChangeTimes[0] = 10;

	cameraStates[1] = CameraState(-80, 220, Vector3(7296.0, 8600.0, 4096.0));
	stateChangeTimes[1] = 17;

	cameraStates[2] = CameraState(-90, 360, Vector3(7296.0, 14000.0, 4096.0));
	stateChangeTimes[2] = 25;

	cameraStates[3] = CameraState(90, 360, Vector3(4296.0, 16000.0, 4096.0));
	stateChangeTimes[3] = 35;

	cameraStates[4] = CameraState(-80, 350, Vector3(4296.0, 1000.0, 4096.0));
	stateChangeTimes[4] = -1;
}