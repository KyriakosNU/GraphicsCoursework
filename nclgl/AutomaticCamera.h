#pragma once
#include "Camera.h"
#include "CameraState.h"

#define MAXSTATES 20

class AutomaticCamera :
    public Camera
{
public:

	AutomaticCamera() {
		AutomaticCamera::InstantiateTrack1();
		isFree = true;
		currentTrack = 1;
		baseSpeed = 30.0f;
		AutomaticCamera::ResetTrackCamera();
	};

	AutomaticCamera(float pitch, float yaw, Vector3 position, bool isFree) {
		AutomaticCamera::InstantiateTrack1();
		this->isFree = isFree;
		currentTrack = 1;
		AutomaticCamera::ResetTrackCamera();
		baseSpeed = 30.0f;

		if (isFree)
		{ 
			this->pitch = pitch;
			this->yaw = yaw;
			this->position = position;
		}

	}

	~AutomaticCamera(void) {};

    bool getIsFree() const { return isFree; }
	void toggleIsFree();

	void UpdateCamera(float dt);
	void UpdateFreeCamera(float dt);
	void UpdateTrackCamera(float dt);
	void ResetTrackCamera();

	void InstantiateTrack1();
	void InstantiateTrack2();

	float getTimeElapsed() const { return timeElapsed; };
	int getCurrentTrack() const { return currentTrack; };
protected:
    bool isFree;
	float timeElapsed;
	int currentState;
	int currentTrack;
	float baseSpeed;
	CameraState cameraStates[MAXSTATES];
	float stateChangeTimes[MAXSTATES];
};

