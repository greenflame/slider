#include "ch.h"
#include "hal.h"
#include "axis.h"

#include <math.h>


#define AXIS0_MAX_POSITION      34600
#define AXIS0_STEPS_PER_METER   40000   // steps  [34500 steps / 862 mm = 40 per mm]
#define AXIS0_STEPS_PER_SECOND  500     // steps

#define TIMER_PRE_FREQ          1000000



// static int stepTable[] = {0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09};

static int axisSteps[AXIS_NUM];
static int axisState[AXIS_NUM];
static int axisPosition[AXIS_NUM]; // текущее положение в шагах
static int axisDestPos[AXIS_NUM]; // в метрах
static int axisDirection[AXIS_NUM];


void axisMove(int axis, int steps);

/*
 * Прерывание таймера TIM4
 */
CH_IRQ_HANDLER(VectorB8)
{
   CH_IRQ_PROLOGUE();

  uint32_t sr = STM32_TIM4->SR; // save TIM4 status register

  if (sr & 0x0001) { // UE (overflow/underflow)
    STM32_TIM4->SR = sr & ~0x0001; // clear UE

    if(axisSteps[AXIS0] > 0) {
      if(axisDirection[AXIS0] > 0) {
        axisPosition[AXIS0]++;
        if(axisPosition[AXIS0] > AXIS0_MAX_POSITION) {
          axisPosition[AXIS0] = AXIS0_MAX_POSITION;
          axisStop(AXIS0);
        }
      } else {
        axisPosition[AXIS0]--;
        if(axisPosition[AXIS0] < 0) {
          axisPosition[AXIS0] = 0;
          axisStop(AXIS0);
        }
      }
      axisSteps[AXIS0]--;
      palSetPad(GPIOB, GPIOB_MA_CLK);
      palClearPad(GPIOB, GPIOB_MA_CLK);
    } else {
      axisStop(AXIS0);
    }

    // if(mtrSteps[1] != 0) {
    //   mtrSteps[1]--;
    //   mtrStepIdx[1] += mtrDir[1];
    //   palWriteGroup(GPIOA, 0x000F, 2, stepTable[mtrStepIdx[1] % 8]);
    // } else {
    //   mtrState[1] = STATE_MTR_STOP;
    // }
  }

  CH_IRQ_EPILOGUE();
}


/*
 * Инициализация таймера и прерывания от него
 */
void axisInit()
{
  /* AXIS0 init */
  palSetPad(GPIOB, GPIOB_MA_EN);

  chSysDisable();
  RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
  chSysEnable();

  TIM4->CR1 = 0;
  TIM4->CR1 |= TIM_CR1_URS;

  nvicEnableVector(TIM4_IRQn, 10);

  TIM4->ARR = TIMER_PRE_FREQ/AXIS0_STEPS_PER_SECOND;
  TIM4->PSC = STM32_HSECLK * STM32_PLLMUL_VALUE / TIMER_PRE_FREQ -1; // 72-1 1000000 Hz
  TIM4->SR = 0;
  TIM4->EGR = TIM_EGR_UG;
  TIM4->DIER |= TIM_DIER_UIE;

  /* AXIS1 init */
  // palWriteGroup(GPIOA, 0x000F, 4, 0);

  /* AXIS2 init */
  //...
}


void axisSetEnable(int axis, int enable)
{
  switch(axis) {
  case AXIS0:
    if(enable == AXIS_ENABLE) {
      palClearPad(GPIOB, GPIOB_MA_EN);
    } else {
      palSetPad(GPIOB, GPIOB_MA_EN);
    }
    break;
  }
}


int axisGetEnable(int axis)
{
  switch(axis) {
  case AXIS0:
    // return (!palReadPad(GPIOB, GPIOB_MA_EN)) & 1;
    return palReadLatch(GPIOB);

  default:
    return 0;
  }
}


/*
 * Установка скорости
 */
void axisSetSpeed(int axis, float speed)
{
  switch(axis) {
  case AXIS0:
    TIM4->ARR = TIMER_PRE_FREQ / (int)(speed * AXIS0_STEPS_PER_METER);
    break;
  }
}


float axisGetSpeed(int axis)
{
  switch(axis) {
  case AXIS0:
    return TIMER_PRE_FREQ / TIM4->ARR / AXIS0_STEPS_PER_METER;
  default:
    return 0;
  }
}


/*
 * Установка нулевой координаты
 */
void axisSetZero(int axis)
{
  if(axis >= 0 && axis < AXIS_NUM)
    axisPosition[axis] = 0;
}


/*
 * Перемещение абсолютное, на заданную позицию
 */
void axisSetDestinationPos(int axis, float pos)
{
  switch(axis) {
  case AXIS0:
    axisDestPos[AXIS0] = pos;
    axisMove(axis, (int)(pos * AXIS0_STEPS_PER_METER) - axisPosition[AXIS0]);
    break;
  }
}


float axisGetDestinationPos(int axis)
{
  if(axis >= 0 && axis < AXIS_NUM) {
    return axisDestPos[axis];
  } else {
    return 0; //???
  }
}


/*
 * Текущая позиция
 */
float axisGetCurrentPos(int axis)
{
  if(axis >= 0 && axis < AXIS_NUM) {
    return axisPosition[axis] / AXIS0_STEPS_PER_METER;
  } else {
    return 0; //???
  }
}


/*
 * Перемещение относительное в шагах
 */
void axisMove(int axis, int steps)
{
  switch(axis) {
  case AXIS0:
    if(steps > 0) {
      palClearPad(GPIOB, GPIOB_MA_DIR);
      axisDirection[AXIS0] = 1;
    } else {
      steps = -steps;
      palSetPad(GPIOB, GPIOB_MA_DIR);
      axisDirection[AXIS0] = -1;
    }
    palClearPad(GPIOB, GPIOB_MA_EN);
    axisSteps[AXIS0] = steps;
    TIM4->CR1 |= TIM_CR1_CEN;
    axisState[AXIS0] = AXIS_STATE_RUN;
    break;
  }
}


/*
 * Останов оси
 */
void axisStop(int axis)
{
  switch(axis) {
  case AXIS0:
    TIM4->CR1 &= ~TIM_CR1_CEN;
    axisState[AXIS0] = AXIS_STATE_STOP;
    break;
  }
}


/*
 * Останов всех осей
 */
void axisStopAll()
{
  /* AXIS0 */
  TIM4->CR1 &= ~TIM_CR1_CEN;
  axisState[AXIS0] = AXIS_STATE_STOP;
  /* AXIS1,2 */
  //...
}



float axisGetMotionTime(int axis, float posFrom, float posTo)
{
  switch(axis) {
  case AXIS0:
    return fabs(posFrom - posTo) * AXIS0_STEPS_PER_METER / AXIS0_STEPS_PER_SECOND;

  default:
    return 0;
  }
}



/*
 * Статус работы мотора
 */
int axisGetState(int axis)
{
  if(axis >= 0 && axis < AXIS_NUM) {
    return axisState[axis];
  } else {
    return AXIS_INVALID; //???
  }
}

