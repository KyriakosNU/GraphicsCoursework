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
	{
		baseSpeed = 30.0f;
	}
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
	baseSpeed = 400;
	cameraStates[0] = CameraState(-18, 125, Vector3(10213.0, 1163.0, 5902.0));
	stateChangeTimes[0] = 6;

	cameraStates[1] = CameraState(-10, 200, Vector3(7511.0,1111.0, 4553.0));
	stateChangeTimes[1] = 13;

	cameraStates[2] = CameraState(-35, 250, Vector3(5311.0, 1300.0, 6340.0));
	stateChangeTimes[2] = 20;

	cameraStates[3] = CameraState(10, 316, Vector3(6396.0, 500.0, 9196.0));
	stateChangeTimes[3] = 26;

	cameraStates[4] = CameraState(-10, 350, Vector3(7996.0, 740.0, 11596.0));
	stateChangeTimes[4] =33;

	cameraStates[5] = CameraState(-10, 80, Vector3(11000.0, 950.0, 10596.0));
	stateChangeTimes[5] = 38;

	cameraStates[6] = CameraState(-30, 95, Vector3(13800.0, 1600.0, 7626.0));
	stateChangeTimes[6] = 45;

	cameraStates[7] = CameraState(-18, 125, Vector3(10213.0, 1163.0, 5902.0));
	stateChangeTimes[7] = -1;
}

void AutomaticCamera::InstantiateTrack2()
{
	currentTrack = 2;

	baseSpeed = 600;
	cameraStates[0] = CameraState(10, 180, Vector3(8910.0, 750.0, 4230.0));
	stateChangeTimes[0] = 5;

	cameraStates[1] = CameraState(-60, 170, Vector3(7296.0, 8600.0, 4096.0));
	stateChangeTimes[1] = 12;

	cameraStates[2] = CameraState(-70, 160, Vector3(7296.0, 14000.0, 4096.0));
	stateChangeTimes[2] = 25;

	cameraStates[3] = CameraState(20, 180, Vector3(7296.0, 55000.0, 4096.0));
	stateChangeTimes[3] = 40;

	cameraStates[4] = CameraState(-70, 160, Vector3(7296.0, 8600.0, 4096.0));
	stateChangeTimes[4] = 55;

	cameraStates[5] = CameraState(-15, 205, Vector3(1627.0, 3956.0, 2602.0));
	stateChangeTimes[5] = -1;

;
}