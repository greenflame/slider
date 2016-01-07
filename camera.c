#include "camera.h"

#include "hal.h"

#define SHUTTER_PULSE         100   /* Длина импульса, мс */


int camera_shoot()
{
  palClearPad(GPIOA, GPIOA_SHUTTER);
  palClearPad(GPIOC, GPIOC_LED);
  chThdSleepMilliseconds(SHUTTER_PULSE);
  palSetPad(GPIOA, GPIOA_SHUTTER);
  palSetPad(GPIOC, GPIOC_LED);

  return 0;
}