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
	if(!isFree)
		AutomaticCamera::ResetTrackCamera();
}

void AutomaticCamera::setIsFree(bool isFree)
{
	this->isFree = isFree;
	if (!this->isFree)
		AutomaticCamera::ResetTrackCamera();
}

void AutomaticCamera::UpdateCamera(float dt) {
	if (Window::GetKeyboard()->KeyDown(KEYBOARD_1))
		setIsFree(false);

	if (Window::GetKeyboard()->KeyDown(KEYBOARD_2))
		setIsFree(true);

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

	float speed = 30.0f * dt;

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

	if (Window::GetKeyboard()->KeyDown(KEYBOARD_SHIFT)) {
		position.y += speed;
	}

	if (Window::GetKeyboard()->KeyDown(KEYBOARD_SPACE)) {
		position.y -= speed;
	}
}

void AutomaticCamera::UpdateTrackCamera(float dt)
{
	timeElapsed += dt;

	if (timeElapsed >= stateChangeTimes[currentState])
		if (currentState + 1 < STATEAMMOUNT)
			currentState++;
		else
			ResetTrackCamera();

	float moveSpeed = dt * 100;
	
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

void AutomaticCamera::InstantiateTrack()
{
	
	cameraStates[0] = CameraState(20, 30, Vector3(4296.0, 600.0, 4096.0));
	stateChangeTimes[0] = 10;

	cameraStates[1] = CameraState(-20, 350, Vector3(4296.0, 500.0, 4096.0));
	stateChangeTimes[1] = 15;

	cameraStates[2] = CameraState(20, 0, Vector3(4296.0, 600.0, 4096.0));
	stateChangeTimes[2] = 20;

	cameraStates[3] = CameraState(20, 0, Vector3(4296.0, 600.0, 4096.0));
	stateChangeTimes[3] = 25;

	cameraStates[4] = CameraState(20, 0, Vector3(4296.0, 600.0, 4096.0));
}