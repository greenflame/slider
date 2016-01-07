
#include "ch.h"
#include "hal.h"
#include "axis.h"


#define AXIS_NUM                3

<<<<<<< HEAD
#define AXIS1_DEF_SPEED         500
#define AXIS1_MAX_POSITION      34600
=======
#define AXIS1_DEF_SPEED       500
#define AXIS1_MAX_POSITION    34600
>>>>>>> 698f4f5ea61e846e3f064761576705499396f898
#define AXIS1_STEPS_PER_METER   40000   // steps  [34500 steps / 862 mm = 40 per mm]
#define AXIS1_STEPS_PER_SECOND  1000    // steps

#define AXIS_STATE_STOP         0
#define AXIS_STATE_RUN          1

#define PRE_FREQ                1000000









// static int stepTable[] = {0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09};

static int axisSteps[AXIS_NUM];
static int axisState[AXIS_NUM];
static int axisPosition[AXIS_NUM];
static int axisDirection[AXIS_NUM];

/*
 * Прерывание таймера TIM4
 */
CH_IRQ_HANDLER(VectorB8)
{
   CH_IRQ_PROLOGUE();

  uint32_t sr = STM32_TIM4->SR; // save TIM4 status register

  if (sr & 0x0001) { // UE (overflow/underflow)
    STM32_TIM4->SR = sr & ~0x0001; // clear UE

    if(axisSteps[AXIS_LINE] > 0) {
      if(axisDirection[AXIS_LINE] > 0) {
        axisPosition[AXIS_LINE]++;
        if(axisPosition[AXIS_LINE] > AXIS_LINE_POS_MAX) {
          axisPosition[AXIS_LINE] = AXIS_LINE_POS_MAX;
          motoStop(AXIS_LINE);
        }
      } else {
        axisPosition[AXIS_LINE]--;
        if(axisPosition[AXIS_LINE] < 0) {
          axisPosition[AXIS_LINE] = 0;
          motoStop(AXIS_LINE);
        }
      }
      axisSteps[AXIS_LINE]--;
      palSetPad(GPIOB, GPIOB_MA_CLK);
      palClearPad(GPIOB, GPIOB_MA_CLK);
    } else {
      motoStop(AXIS_LINE);
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
void motoInit(int moto)
{
  switch(moto) {
  case AXIS_LINE:
    palSetPad(GPIOB, GPIOB_MA_EN);

    chSysDisable();
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    chSysEnable();

    TIM4->CR1 = 0;
    TIM4->CR1 |= TIM_CR1_URS;

    nvicEnableVector(TIM4_IRQn, 10);

    TIM4->ARR = PRE_FREQ/AXIS_LINE_DEF_SPEED;
    TIM4->PSC = STM32_HSECLK * STM32_PLLMUL_VALUE / PRE_FREQ -1; // 72-1 1000000 Hz
    TIM4->SR = 0;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->DIER |= TIM_DIER_UIE;
    break;

  case AXIS_YAW:
    // palWriteGroup(GPIOA, 0x000F, 4, 0);
    break;

  case AXIS_ROLL:
    break;
  }
}


/*
 * Установка скорости
 */
void setMotoSpeed(int moto, int speed)
{
  switch(moto) {
  case AXIS_LINE:
    TIM4->ARR = PRE_FREQ/speed;
    break;

  case AXIS_YAW:
    break;

  case AXIS_ROLL:
    break;
  }
}


/*
 * Установка нулевой координаты
 */
void setMotoZero(int moto)
{
  switch(moto) {
  case AXIS_LINE:
    axisPosition[AXIS_LINE] = 0;
    break;

  case AXIS_YAW:
    break;

  case AXIS_ROLL:
    break;
  }
}


/*
 * Перемещение абсолютное, на заданную позицию
 */
void motoGoPos(int moto, int pos)
{
  int steps;
  switch(moto) {
  case AXIS_LINE:
    steps = pos - axisPosition[AXIS_LINE];
    motoMove(moto, steps);
    break;

  case AXIS_YAW:
    break;

  case AXIS_ROLL:
    break;
  }
}


/*
 * Перемещение относительное в шагах
 */
void motoMove(int moto, int steps)
{
  switch(moto) {
  case AXIS_LINE:
    if(steps > 0) {
      palClearPad(GPIOB, GPIOB_MA_DIR);
      axisDirection[AXIS_LINE] = 1;
    } else {
      steps = -steps;
      palSetPad(GPIOB, GPIOB_MA_DIR);
      axisDirection[AXIS_LINE] = -1;
    }
    palClearPad(GPIOB, GPIOB_MA_EN);
    axisSteps[AXIS_LINE] = steps;
    TIM4->CR1 |= TIM_CR1_CEN;
    axisState[AXIS_LINE] = AXIS_STATE_RUN;
    break;

  case AXIS_YAW:
    break;

  case AXIS_ROLL:
    break;
  }
}


/*
 * Останов мотора
 */
void motoStop(int moto)
{
  switch(moto) {
  case AXIS_LINE:
    TIM4->CR1 &= ~TIM_CR1_CEN;
    axisState[AXIS_LINE] = AXIS_STATE_STOP;
    break;

  case AXIS_YAW:
    break;

  case AXIS_ROLL:
    break;
  }
}


/*
 * Статус работы мотора
 */
int getMotoState(int moto)
{
  return axisState[moto];
}


/*
 * Текущая позиция
 */
int getMotoPos(int moto)
{
  return axisPosition[moto];
}
