#ifndef CAMERA_H
#define CAMERA_H

void cameraShoot(void);

void cameraSetExposure(float time);
float cameraGetExposure(void);

void cameraSetStabilizationTime(float time);
float cameraGetStabilizationTime(void);

#endif