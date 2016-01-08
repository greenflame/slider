#include "scripts.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ch.h"
#include "hal.h"
#include "shell.h"
#include "chprintf.h"
#include "evtimer.h"
#include "ff.h"

#include "axis.h"
#include "camera.h"

#define SLEEP_INTERVAL      10      // ticks
#define COORDINATES_COUNT   (AXIS_NUM + 1)
#define SCRIPT_MAX_LENGTH   128
#define SPLIT_TOKENS        "[;]"

/*
 * For recursion
 */
float parseAdd(char **str, float n);
float parseGroup(char **str, float n);

/*
 * Solver main function
 */
float solveExpr(char *expr, float n)
{
    char *exprCopy = expr;
    return parseAdd(&exprCopy, n);
}

/*
 * Extract step count from script
 */
int solveSteps(char *script)    // check
{
    char scriptCpy[128];
    strcpy(scriptCpy, script);

    return atoi(strtok(scriptCpy, SPLIT_TOKENS));
}

/*
 * Solve coorginates(time, axis positions) fo current step
 */
void solveCoordinates(char *script, int step, float *coordinates, int coordinatesCount)
{
    char scriptCpy[128];
    strcpy(scriptCpy, script);

    strtok(scriptCpy, SPLIT_TOKENS);   // Ignore steps count

    int i;
    for (i = 0; i < coordinatesCount; i++)
    {
        char *curCoordinate = strtok(NULL, SPLIT_TOKENS);
        coordinates[i] = solveExpr(curCoordinate, step);
    }
}

/*
 * Print float as int
 */
int ftoi(float in)
{
    return (int)(in * 1000);
}

/*
 * Check script for time interval correctness
 */
int scriptsCheck(char *script, BaseSequentialStream *out)
{
    chprintf(out, "Checking script...\r\n");

    int shots = solveSteps(script);

    int i;
    for (i = 1; i < shots; i++)
    {
        // Calculating coordinates
        float prevCoordinates[COORDINATES_COUNT];
        float curCoordinates[COORDINATES_COUNT];

        solveCoordinates(script, i - 1, prevCoordinates, COORDINATES_COUNT);
        solveCoordinates(script, i, curCoordinates, COORDINATES_COUNT);

        // Calculating max motion time
        float maxMotionTime = 0;
        int slowestAxis = -1;
        int j;
        for (j = 0; j < AXIS_NUM; j++)
        {
            float curAxisMotionTime = axisGetMotionTime(j, prevCoordinates[j + 1], curCoordinates[j + 1]);

            if (curAxisMotionTime > maxMotionTime)
            {
                maxMotionTime = curAxisMotionTime;
                slowestAxis = j;
            }
        }

        // Check time correctness
        if (curCoordinates[0] - prevCoordinates[0] < maxMotionTime + cameraGetExposure() + cameraGetStabilizationTime())
        {
            chprintf(out, "Axis %d cann't be in time at step %d.\r\n\r\n", slowestAxis, i);

            chprintf(out, "Free time: %d ms\r\n", ftoi(curCoordinates[0] - prevCoordinates[0]));
            chprintf(out, "Motion time: %d ms\r\n", ftoi(maxMotionTime));
            chprintf(out, "Shot time: %d ms\r\n", ftoi(cameraGetExposure()));
            chprintf(out, "Stabilization time: %d ms\r\n", ftoi(cameraGetStabilizationTime()));
            chprintf(out, "Time limit: %d ms\r\n", ftoi(prevCoordinates[0] - curCoordinates[0]
                + maxMotionTime + cameraGetExposure() + cameraGetStabilizationTime()));

            return 1;
        }
    }

    chprintf(out, "Ok.\r\n");
    return 0;
}

/*
 * Script format: [shots;time_expr;axis0_expr] (time in seconds, location in meters)
 */
int scriptsExecute(char *script, BaseSequentialStream *out)
{
    // Checking script
    if (scriptsCheck(script, out))
    {
        return 1;   // Error
    }

    int shots = solveSteps(script);
    systime_t startTime;

    int i;
    for (i = 0; i < shots; i++)
    {
        // Calculating coordinates
        float curCoordinates[COORDINATES_COUNT];
        solveCoordinates(script, i, curCoordinates, COORDINATES_COUNT);

        if (i)  // Not first shot
        {
            float prevTime;
            solveCoordinates(script, i - 1, &prevTime, 1);  // Solve only time function(first)

            // Wait for | shot time + free time = interval - stabilization - move
            while (chVTGetSystemTime() - startTime < (systime_t)((prevTime + cameraGetExposure()) * CH_CFG_ST_FREQUENCY))
            {
                chThdSleepMilliseconds(SLEEP_INTERVAL);
            }

            // Move axis
            int j;
            for (j = 0; j < AXIS_NUM; j++)
            {
                axisSetDestinationPos(j, curCoordinates[j + 1]);
            }

            // Wait for shot
            while (chVTGetSystemTime() - startTime < (systime_t)(curCoordinates[0] * CH_CFG_ST_FREQUENCY))
            {
                chThdSleepMilliseconds(SLEEP_INTERVAL);
            }
        }
        else    // First shot
        {
            // Move axis to start position
            int j;
            for (j = 0; j < AXIS_NUM; j++)
            {
                axisSetDestinationPos(j, curCoordinates[j + 1]);
            }

            // Wait for axis
            while (1)
            {
                int isMotion = 0;

                int j;
                for (j = 0; j < AXIS_NUM; j++)
                {
                    if (axisGetState(j) == AXIS_STATE_RUN)
                    {
                        isMotion = 1;
                        break;
                    }
                }

                if (isMotion)
                {
                    chThdSleepMilliseconds(SLEEP_INTERVAL);
                }
                else
                {
                    break;
                }
            }

            // Remember start time
            startTime = chVTGetSystemTime();
        }

        // Take shot
        cameraShoot();

        // Write log
        chprintf(out, "Step: %d, time: %d", i, ftoi(curCoordinates[0]));
        int j;
        for (j = 0; j < AXIS_NUM; j++)
        {
            chprintf(out, ", axis%d: %d", j, ftoi(curCoordinates[j + 1]));
        }
        chprintf(out, "\r\n");
    }

    return 0;
}

/*
 * primitive_value : NUMBER | ID | func_call ;
 */
float parsePrimitiveValue(char **str, float n)
{
    float result = 0;

    switch (**str)
    {
        case 'n':           // n
            (*str)++;
            result = n;
            break;

        case 's':           // sin
            (*str) += 3;
            result = sin(parseGroup(str, n));
            break;
        case 't':           // tan
            (*str) += 3;
            result = tan(parseGroup(str, n));
            break;
        case 'c':           // cos
            (*str) += 3;
            result = cos(parseGroup(str, n));
            break;

        default:            // NUMBER
        {
            char buf[16] = {0};
            int curpos = 0;

            while ((**str >= '0' && **str <= '9') || **str == '.')  // Next char is in "1234567890."
            {
                buf[curpos++] = *((*str)++);
            }

            result = atof(buf);
            break;
        }
    }

    return result;
}

/*
 * value_group : '('! value_add ')'! | primitive_value ;
 */
float parseGroup(char **str, float n)
{
    float result = 0;

    if (**str == '(')
    {
        (*str)++;  // '('
        result = parseAdd(str, n); // value_add
        (*str)++;  // ')'
    }
    else
    {
        result = parsePrimitiveValue(str, n);
    }

    return result;
}

/*
 * value_mult : value_group ( ( MUL | DIV )^ value_group )* ;
 */
float parseMult(char **str, float n)
{
    float result = parseGroup(str, n);

    while (**str == '*' || **str == '/')
    {
        switch (**str)
        {
            case '*':
                (*str)++;
                result *= parseGroup(str, n);
                break;
            case '/':
                (*str)++;
                result /= parseGroup(str, n);
                break;
        }
    }

    return result;
}

/*
 * value_add : value_mult ( ( ADD | SUB )^ value_mult )* ;
 */
float parseAdd(char **str, float n)
{
    float result = parseMult(str, n);

    while (**str == '+' || **str == '-')
    {
        switch (**str)
        {
            case '+':
                (*str)++;
                result += parseMult(str, n);
                break;
            case '-':
                (*str)++;
                result -= parseMult(str, n);
                break;
        }
    }

    return result;
}
