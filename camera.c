#include "camera.h"

#include "hal.h"

#define SHUTTER_PULSE       0.01     // ms

#define DEFAULT_EXPOSURE    0.5     // ms
#define DEFAULT_STAB_TIME   0.2     // ms


float exposure = DEFAULT_EXPOSURE;
float stabilizationTime = DEFAULT_STAB_TIME;


void cameraShoot(void)
{
  palClearPad(GPIOA, GPIOA_SHUTTER);
  palClearPad(GPIOC, GPIOC_LED);
  chThdSleepMilliseconds(SHUTTER_PULSE * CH_CFG_ST_FREQUENCY);
  palSetPad(GPIOA, GPIOA_SHUTTER);
  palSetPad(GPIOC, GPIOC_LED);
}

void cameraSetExposure(float time)
{
  exposure = time;
}

float cameraGetExposure(void)
{
  return exposure;
}

void cameraSetStabilizationTime(float time)
{
  stabilizationTime = time;
}

float cameraGetStabilizationTime(void)
{
  return stabilizationTime;
}
