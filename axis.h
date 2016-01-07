/*
    Slider - Copyright (C) 2015-2016 Kolesov

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef AXIS_H_
#define AXIS_H_
#ifdef __cplusplus
extern "C" {
#endif


  void axisInit(int moto);
  void axisMove(int moto, int steps);
  void axisGoPos(int moto, int pos);
  void motoStop(int moto);
  void setMotoSpeed(int moto, int speed);
  void setMotoZero(int moto);
  int getMotoState(int moto);
  int getMotoPos(int moto);


#ifdef __cplusplus
}
#endif

#endif /* AXIS_H_ */