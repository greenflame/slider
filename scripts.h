#ifndef SCRIPTS_H
#define SCRIPTS_H

#include "ch.h"

int scriptsCheck(char *script, BaseSequentialStream *out);
int scriptsExecute(char *script, BaseSequentialStream *out);

#endif
