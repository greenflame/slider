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
 * Константы
 */

#define TL_STATE_STOP         0
#define TL_STATE_0            1
#define TL_STATE_1            2

#define TL_CMD_READY          0
#define TL_CMD_STOP           1
#define TL_CMD_0              2
#define TL_CMD_1              3


/*
 * Переменные глобальные
 */
static int motoCurrent = MOTO_LINE;

static int tlState = TL_STATE_STOP;
static int tlCmd = TL_CMD_READY;
static int tlSteps = 100;
static unsigned int tlPause = 3000;
static int tlStab = 500;
static int tlShoots = 0;







/*===========================================================================*/
/* Card insertion monitor.                                                   */
/*===========================================================================*/

#define POLLING_INTERVAL                10
#define POLLING_DELAY                   10

/**
 * @brief   Card monitor timer.
 */
static virtual_timer_t tmr;

/**
 * @brief   Debounce counter.
 */
static unsigned cnt;

/**
 * @brief   Card event sources.
 */
static event_source_t inserted_event, removed_event;

/**
 * @brief   Insertion monitor timer callback function.
 *
 * @param[in] p         pointer to the @p BaseBlockDevice object
 *
 * @notapi
 */
static void tmrfunc(void *p) {
  BaseBlockDevice *bbdp = p;

  /* The presence check is performed only while the driver is not in a
     transfer state because it is often performed by changing the mode of
     the pin connected to the CS/D3 contact of the card, this could disturb
     the transfer.*/
  blkstate_t state = blkGetDriverState(bbdp);
  chSysLockFromISR();
  if ((state != BLK_READING) && (state != BLK_WRITING)) {
    /* Safe to perform the check.*/
    if (cnt > 0) {
      if (blkIsInserted(bbdp)) {
        if (--cnt == 0) {
          chEvtBroadcastI(&inserted_event);
        }
      }
      else
        cnt = POLLING_INTERVAL;
    }
    else {
      if (!blkIsInserted(bbdp)) {
        cnt = POLLING_INTERVAL;
        chEvtBroadcastI(&removed_event);
      }
    }
  }
  chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, bbdp);
  chSysUnlockFromISR();
}

/**
 * @brief   Polling monitor start.
 *
 * @param[in] p         pointer to an object implementing @p BaseBlockDevice
 *
 * @notapi
 */
static void tmr_init(void *p) {

  chEvtObjectInit(&inserted_event);
  chEvtObjectInit(&removed_event);

  chSysLock();
  cnt = POLLING_INTERVAL;
  chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, p);
  chSysUnlock();
}

/*===========================================================================*/
/* FatFs related.                                                            */
/*===========================================================================*/

/**
 * @brief FS object.
 */
FATFS MMC_FS;

/**
 * MMC driver instance.
 */
MMCDriver MMCD1;

/* FS mounted and ready.*/
static bool fs_ready = FALSE;

/* Maximum speed SPI configuration (18MHz, CPHA=0, CPOL=0, MSb first).*/
static SPIConfig hs_spicfg = {NULL, IOPORT2, GPIOB_SPI2NSS, 0};

/* Low speed SPI configuration (281.250kHz, CPHA=0, CPOL=0, MSb first).*/
static SPIConfig ls_spicfg = {NULL, IOPORT2, GPIOB_SPI2NSS,
                              SPI_CR1_BR_2 | SPI_CR1_BR_1};

/* MMC/SD over SPI driver configuration.*/
static MMCConfig mmccfg = {&SPID2, &ls_spicfg, &hs_spicfg};

/* Generic large buffer.*/
uint8_t fbuff[1024];

static FRESULT scan_files(BaseSequentialStream *chp, char *path) {
  FRESULT res;
  FILINFO fno;
  DIR dir;
  int i;
  char *fn;

#if _USE_LFN
  fno.lfname = 0;
  fno.lfsize = 0;
#endif
  res = f_opendir(&dir, path);
  if (res == FR_OK) {
    i = strlen(path);
    for (;;) {
      res = f_readdir(&dir, &fno);
      if (res != FR_OK || fno.fname[0] == 0)
        break;
      if (fno.fname[0] == '.')
        continue;
      fn = fno.fname;
      if (fno.fattrib & AM_DIR) {
        path[i++] = '/';
        strcpy(&path[i], fn);
        res = scan_files(chp, path);
        if (res != FR_OK)
          break;
        path[--i] = 0;
      }
      else {
        chprintf(chp, "%s/%s\r\n", path, fn);
      }
    }
  }
  return res;
}




/*
 * Поток для управления таймлапсом
 */
static THD_WORKING_AREA(waTimeLapse, 128);
static THD_FUNCTION(TimeLapse, arg) {

  (void)arg;
  chRegSetThreadName("TimeLapse");

  systime_t st;

  while (TRUE) {
    chThdSleepMilliseconds(10); //???
    switch(tlState) {
    case TL_STATE_STOP:
      if(tlCmd == TL_CMD_0) {
        tlCmd = TL_CMD_READY;
        tlState = TL_STATE_0;
        camera_shoot();
        st = chVTGetSystemTime();
      }
      if(tlCmd == TL_CMD_1) {
        tlCmd = TL_CMD_READY;
        tlState = TL_STATE_1;
        camera_shoot();
        st = chVTGetSystemTime();
      }
      break;

    case TL_STATE_0:
      if(chVTGetSystemTime() - st > tlPause) {
         tlShoots--;
        if(tlShoots != 0) {
          st += MS2ST(tlPause);
          camera_shoot();
        } else {
          tlState = TL_STATE_STOP;
        }
      }
      if(tlCmd == TL_CMD_STOP) {
        tlState = TL_STATE_STOP;
      }
      break;

    case TL_STATE_1:
      if(chVTGetSystemTime() - st > tlPause) {
        tlShoots--;
        if(tlShoots != 0) {
          st += MS2ST(tlPause);
          motoMove(MOTO_LINE, tlSteps);
          while(getMotoState(MOTO_LINE) != MOTO_STATE_STOP)
            chThdSleepMilliseconds(10);
          chThdSleepMilliseconds(tlStab);
          camera_shoot();
        } else {
          tlState = TL_STATE_STOP;
        }
      }
      if(tlCmd == TL_CMD_STOP) {
        tlState = TL_STATE_STOP;
      }
      if((tlSteps > 0 && getMotoPos(MOTO_LINE) + tlSteps > MOTO_LINE_POS_MAX)
        || (tlSteps < 0 && getMotoPos(MOTO_LINE) - tlSteps < 0)) {
        tlState = TL_STATE_STOP;
      }
      break;
    }
  }
}


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

static void cmd_tree(BaseSequentialStream *chp, int argc, char *argv[]) {
  FRESULT err;
  uint32_t clusters;
  FATFS *fsp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: tree\r\n");
    return;
  }
  if (!fs_ready) {
    chprintf(chp, "File System not mounted\r\n");
    return;
  }
  err = f_getfree("/", &clusters, &fsp);
  if (err != FR_OK) {
    chprintf(chp, "FS: f_getfree() failed\r\n");
    return;
  }
  chprintf(chp,
           "FS: %lu free clusters, %lu sectors per cluster, %lu bytes free\r\n",
           clusters, (uint32_t)MMC_FS.csize,
           clusters * (uint32_t)MMC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE);
  fbuff[0] = 0;
  scan_files(chp, (char *)fbuff);
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




static void cmd_m(BaseSequentialStream *chp, int argc, char *argv[]) {
  if (argc != 1) {
    chprintf(chp, "Usage: m <motor>\r\n");
    return;
  }
  motoCurrent = atoi(argv[0]);
}

static void cmd_zero(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: zero\r\n");
    return;
  }
  setMotoZero(motoCurrent);
}

static void cmd_mov(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "Usage: mov <steps>\r\n");
    return;
  }
  motoMove(motoCurrent, atoi(argv[0]));
}

static void cmd_pos(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "Usage: pos <pos>\r\n");
    return;
  }
  motoGoPos(motoCurrent, atoi(argv[0]));
}

static void cmd_speed(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "Usage: speed <speed>\r\n");
    return;
  }
  setMotoSpeed(motoCurrent, atoi(argv[0]));
}

static void cmd_stop(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: stop\r\n");
    return;
  }
  motoStop(motoCurrent);
}


static void cmd_tlstop(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: stop\r\n");
    return;
  }
  tlCmd = TL_CMD_STOP;
}


static void cmd_stat(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: stat\r\n");
    return;
  }
  chprintf(chp, "State: %d Pos: %d\r\n",
    getMotoState(motoCurrent),
    getMotoPos(motoCurrent));
}


static void cmd_on(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: on\r\n");
    return;
  }
  palClearPad(GPIOB, GPIOB_MA_EN);
  chprintf(chp, "Power on\r\n");
}

static void cmd_off(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: off\r\n");
    return;
  }
  palSetPad(GPIOB, GPIOB_MA_EN);
  chprintf(chp, "Power off\r\n");
}


static void cmd_tl0(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc < 1 || argc > 2) {
    chprintf(chp, "Usage: tl0 <pause> [shoots]\r\n");
    return;
  }

  tlPause = atoi(argv[0]);
  if(argc == 2)
    tlShoots = atoi(argv[1]);
  else
    tlShoots = 0;
  tlCmd = TL_CMD_0;
}


static void cmd_tl1(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc < 3 || argc > 4) {
    chprintf(chp, "Usage: tl1 <steps> <stab> <pause> [shoots]\r\n");
    return;
  }

  tlSteps = atoi(argv[0]);
  tlStab = atoi(argv[1]);
  tlPause = atoi(argv[2]);
  if(argc == 4)
    tlShoots = atoi(argv[3]);
  else
    tlShoots = 0;
  tlCmd = TL_CMD_1;
}


static void cmd_tlst(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: tlst\r\n");
    return;
  }

  chprintf(chp, "TL state: %d shoot: %d\r\n", tlState, tlShoots);
}


static void cmd_time(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: time\r\n");
    return;
  }

  chprintf(chp, "Time: %d\r\n", chVTGetSystemTimeX());
}


static void cmd_mount(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: time\r\n");
    return;
  }

  FRESULT err;

  if (mmcConnect(&MMCD1)) {
    chprintf(chp, "Already mounted\r\n");
    return;
  }
  err = f_mount(&MMC_FS, "/", 1);
  if (err != FR_OK) {
    chprintf(chp, "Mount error: %d\r\n", err);
    mmcDisconnect(&MMCD1);
    return;
  }
  chprintf(chp, "Mounted\r\n");
  fs_ready = TRUE;
}


static void cmd_unmount(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc != 0) {
    chprintf(chp, "Usage: time\r\n");
    return;
  }

  mmcDisconnect(&MMCD1);
  chprintf(chp, "Unmounted\r\n");
  fs_ready = FALSE;
}


static const ShellCommand commands[] = {
  {"tree", cmd_tree},
  {"mount", cmd_mount},
  {"unmount", cmd_unmount},
  {"mem", cmd_mem},
  {"m", cmd_m},
  {"zero", cmd_zero},
  {"mov", cmd_mov},
  {"pos", cmd_pos},
  {"speed", cmd_speed},
  {"stop", cmd_stop},
  {"tlstop", cmd_tlstop},
  {"stat", cmd_stat},
  {"on", cmd_on},
  {"off", cmd_off},
  {"tl0", cmd_tl0},
  {"tl1", cmd_tl1},
  {"tlst", cmd_tlst},
  {"time", cmd_time},
  {"execute", cmd_execute}, // Execute script
  {"check", cmd_check},     // Check script
  {NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SD1,
  commands
};



/*
 * MMC card insertion event.
 */
static void InsertHandler(eventid_t id) {
  FRESULT err;

  (void)id;
  /*
   * On insertion MMC initialization and FS mount.
   */
  if (mmcConnect(&MMCD1)) {
    return;
  }
  err = f_mount(&MMC_FS, "/", 1);
  if (err != FR_OK) {
    mmcDisconnect(&MMCD1);
    return;
  }
  fs_ready = TRUE;
}

/*
 * MMC card removal event.
 */
static void RemoveHandler(eventid_t id) {

  (void)id;
  mmcDisconnect(&MMCD1);
  fs_ready = FALSE;
}





/*
 * Application entry point.
 */
int main(void) {

  static const evhandler_t evhndl[] = {
    InsertHandler,
    RemoveHandler
  };
  struct event_listener el0, el1;


  halInit();
  chSysInit();

  sdStart(&SD1, NULL);
  chprintf((BaseSequentialStream*)&SD1, "\r\n\r\nSLIDER Version 0.1\r");

  /* Shell manager initialization */
  shellInit();
  shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);


  /*
   * Initializes the MMC driver to work with SPI2.
   */
  palSetPadMode(IOPORT2, GPIOB_SPI2NSS, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPad(IOPORT2, GPIOB_SPI2NSS);
  mmcObjectInit(&MMCD1);
  mmcStart(&MMCD1, &mmccfg);


  /*
   * Activates the card insertion monitor.
   */
  tmr_init(&MMCD1);



  /* Создаем потоки */
  // chThdCreateStatic(waLedFlash, sizeof(waLedFlash), NORMALPRIO, LedFlash, NULL);
  chThdCreateStatic(waTimeLapse, sizeof(waTimeLapse), NORMALPRIO, TimeLapse, NULL);


  /* Инициализация моторов */
  motoInit(MOTO_LINE);


  /* Ничего не делаем... */

  chEvtRegister(&inserted_event, &el0, 0);
  chEvtRegister(&removed_event, &el1, 1);
  while (TRUE) {
    // chThdSleepMilliseconds(1000);
    chEvtDispatch(evhndl, chEvtWaitOneTimeout(ALL_EVENTS, MS2ST(500)));
  }

}


