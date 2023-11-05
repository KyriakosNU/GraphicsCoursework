#pragma once
# include "Vector3.h"
class CameraState
{
public:
	CameraState(void) {
		yaw = 0.0f;
		pitch = 0.0f;
	};

	CameraState(float pitch, float yaw, Vector3 position) {
		this->pitch = pitch;
		this->yaw = yaw;
		this->position = position;
	}

	~CameraState(void) {};

	Vector3 GetPosition() const { return position; }
	void SetPosition(Vector3 val) { position = val; }

	float GetYaw() const { return yaw; }
	void SetYaw(float y) { yaw = y; }

	float GetPitch() const { return pitch; }
	void SetPitch(float p) { pitch = p; }

protected:
	float yaw;
	float pitch;
	Vector3 position;
};

