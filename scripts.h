#ifndef SCRIPTS_H
#define SCRIPTS_H

#include "ch.h"

int scripts_check(char *script, BaseSequentialStream *out);
int scripts_execute(char *script, BaseSequentialStream *out);

#endif