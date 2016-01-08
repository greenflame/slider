#ifndef AXIS_H_
#define AXIS_H_
#ifdef __cplusplus
extern "C" {
#endif

#define AXIS_NUM                1

#define AXIS0                   0
#define AXIS1                   1
#define AXIS2                   2

#define AXIS_STATE_STOP         0
#define AXIS_STATE_RUN          1

#define AXIS_ENABLE             1
#define AXIS_DISABLE            0

#define AXIS_INVALID           -1


  void axisInit(void);

  void axisSetEnable(int axis, int enable);
  int axisGetEnable(int axis);

  void axisSetDestinationPos(int axis, float pos);
  float axisGetDestinationPos(int axis);

  float axisGetCurrentPos(int axis);

  void axisStop(int axis);
  void axisStopAll(void);

  void axisSetSpeed(int axis, float speed);
  float axisGetSpeed(int axis);

  float axisGetMotionTime(int axis, float posFrom, float posTo);

  void axisSetZero(int axis);

  int axisGetState(int axis);


#ifdef __cplusplus
}
#endif

#endif /* AXIS_H_ */