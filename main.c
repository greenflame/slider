#include <stdlib.h>
#include <string.h>

#include "ch.h"
#include "hal.h"
#include "shell.h"
#include "chprintf.h"
#include "evtimer.h"
#include "ff.h"

#include "axis.h"
#include "scripts.h"
#include "camera.h"


/*
 * Command line related.
 */
#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

static void cmd_check(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "Usage: check <programm>\r\n");
    return;
  }

  // scripts_execute(argv[0], chp);
  scripts_check(argv[0], chp);
}

static void cmd_execute(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "Usage: execute <programm>\r\n");
    return;
  }

  // scripts_execute(argv[0], chp);
  scripts_execute(argv[0], chp);
}


static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
  size_t n, size;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreGetStatusX());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}


static void cmd_zero(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: zero\r\n");
    return;
  }
  axisSetZero(AXIS0);
}

static void cmd_pos(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "Usage: pos <pos>\r\n");
    return;
  }
  axisSetDestinationPos(AXIS0, atoi(argv[0])/1000);
}

static void cmd_speed(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "Usage: speed <speed>\r\n");
    return;
  }
  axisSetSpeed(AXIS0, atoi(argv[0])/1000);
}

static void cmd_stop(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: stop\r\n");
    return;
  }
  axisStop(AXIS0);
}


static void cmd_stat(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: stat\r\n");
    return;
  }
  chprintf(chp, "State: %d Pos: %d\r\n",
    axisGetState(AXIS0),
    axisGetCurrentPos(AXIS0));
}


static void cmd_on(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: on\r\n");
    return;
  }
  axisSetEnable(AXIS0, 1);
  chprintf(chp, "Power on\r\n");
}

static void cmd_off(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: off\r\n");
    return;
  }
  axisSetEnable(AXIS0, 0);
  chprintf(chp, "Power off\r\n");
}


static const ShellCommand commands[] = {
  {"mem", cmd_mem},
  {"zero", cmd_zero},
  {"pos", cmd_pos},
  {"speed", cmd_speed},
  {"stop", cmd_stop},
  {"stat", cmd_stat},
  {"on", cmd_on},
  {"off", cmd_off},
  {"execute", cmd_execute}, // Execute script
  {"check", cmd_check},     // Check script
  {NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SD1,
  commands
};




/*
 * Application entry point.
 */
int main(void) {

  halInit();
  chSysInit();

  sdStart(&SD1, NULL);
  chprintf((BaseSequentialStream*)&SD1, "\r\n\r\nSLIDER Version 0.2\r");

  /* Shell manager initialization */
  shellInit();
  shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);


    /* Создаем потоки */
  // chThdCreateStatic(waLedFlash, sizeof(waLedFlash), NORMALPRIO, LedFlash, NULL);
  // chThdCreateStatic(waTimeLapse, sizeof(waTimeLapse), NORMALPRIO, TimeLapse, NULL);


  /* Инициализация моторов */
  axisInit();


  /* Ничего не делаем... */

  // chEvtRegister(&inserted_event, &el0, 0);
  // chEvtRegister(&removed_event, &el1, 1);
  while (TRUE) {
    chThdSleepMilliseconds(1000);
    // chEvtDispatch(evhndl, chEvtWaitOneTimeout(ALL_EVENTS, MS2ST(500)));
  }

}


