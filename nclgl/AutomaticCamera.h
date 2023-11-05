#pragma once
#include "Camera.h"
#include "CameraState.h"

#define STATEAMMOUNT 5

class AutomaticCamera :
    public Camera
{
public:

	AutomaticCamera() {
		AutomaticCamera::InstantiateTrack();
		isFree = false;
		AutomaticCamera::ResetTrackCamera();
	};

	AutomaticCamera(float pitch, float yaw, Vector3 position, bool isFree) {
		AutomaticCamera::InstantiateTrack();
		this->isFree = isFree;

		if (isFree)
		{ 
			this->pitch = pitch;
			this->yaw = yaw;
			this->position = position;
		}
		else
			AutomaticCamera::ResetTrackCamera();
	}

	~AutomaticCamera(void) {};

    bool getIsFree() const { return isFree; }
	void toggleIsFree();

	void UpdateCamera(float dt);
	void UpdateFreeCamera(float dt);
	void UpdateTrackCamera(float dt);
	void ResetTrackCamera();

	void InstantiateTrack();

protected:
    bool isFree;
	float timeElapsed;
	int currentState;
	CameraState cameraStates[STATEAMMOUNT];
	float stateChangeTimes[STATEAMMOUNT];
};

