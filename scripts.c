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

#define SLEEP_INTERVAL          10      // ticks

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
    char *expr_copy = expr;
    return parseAdd(&expr_copy, n);
}

/*
 * Check script for time interval correctness
 */
int scriptsCheck(char *script, BaseSequentialStream *out)
{
    chprintf(out, "Checking script...\r\n");

    // Local copy(split corrupts string)
    char script_cpy[128];
    strcpy(script_cpy, script);

    // Splitting expression
    int shots = atoi(strtok(script_cpy, "[;]"));
    char *time_expr = strtok(NULL, "[;]");
    char *axis0_expr = strtok(NULL, "[;]");

    int i;
    for (i = 1; i < shots; i++)
    {
        // Calculating coordinates
        float prev_time  = solveExpr(time_expr, i - 1);
        float prev_axis0 = solveExpr(axis0_expr, i - 1);

        float cur_time = solveExpr(time_expr, i);
        float cur_axis0 = solveExpr(axis0_expr, i);

        float axis0_move_time = axisGetMotionTime(AXIS0, prev_axis0, cur_axis0);

        // Check time correctness
        if (cur_time - prev_time < axis0_move_time + cameraGetExposure() + cameraGetStabilizationTime())  // axis0
        {
            chprintf(out, "Axis1 cann't be in time.\r\n\r\n");

            chprintf(out, "Previous step: %d, time: %d, pos: %d\r\n", i - 1, (int)(prev_time * 1000), (int)(prev_axis0 * 1000));
            chprintf(out, "Current step: %d, time: %d, pos: %d\r\n\r\n", i, (int)(cur_time * 1000), (int)(cur_axis0 * 1000));


            chprintf(out, "Free time: %d ms\r\n", (int)((cur_time - prev_time) * 1000));
            chprintf(out, "Move time: %d ms\r\n", (int)(axis0_move_time * 1000));
            chprintf(out, "Shot time: %d ms\r\n", (int)(cameraGetExposure() * 1000));
            chprintf(out, "Stabilization time: %d ms\r\n", (int)(cameraGetStabilizationTime() * 1000));
            chprintf(out, "Time defect: %d ms\r\n", (int)(( (axis0_move_time + cameraGetExposure() + cameraGetStabilizationTime()) - (cur_time - prev_time) ) * 1000));

            return 1;   // Error
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

    // Local copy(split corrupts string)
    char script_cpy[128];
    strcpy(script_cpy, script);

    // Splitting expression
    int shots = atoi(strtok(script_cpy, "[;]"));
    char *time_expr = strtok(NULL, "[;]");
    char *axis0_expr = strtok(NULL, "[;]");

    systime_t start_time;

    int i;
    for (i = 0; i < shots; i++)
    {
        // Calculating coordinates
        float cur_time = solveExpr(time_expr, i);
        float cur_axis0 = solveExpr(axis0_expr, i);

        if (i)  // Not first shot
        {
            float prev_time  = solveExpr(time_expr, i - 1);

            // Wait for | shot time + free time = interval - stabilization - move
            while (chVTGetSystemTime() - start_time < (systime_t)((prev_time + cameraGetExposure()) * CH_CFG_ST_FREQUENCY))
            {
                chThdSleepMilliseconds(SLEEP_INTERVAL);
            }

            // Move axis
            axisSetDestinationPos(AXIS0, cur_axis0);

            // Wait for shot
            while (chVTGetSystemTime() - start_time < (systime_t)(cur_time * CH_CFG_ST_FREQUENCY))
            {
                chThdSleepMilliseconds(SLEEP_INTERVAL);
            }
        }
        else    // First shot
        {
            // Move axis to start position
            axisSetDestinationPos(AXIS0, cur_axis0);

            // Wait for axis
            while (axisGetState(AXIS0) == AXIS_STATE_RUN)
            {
                chThdSleepMilliseconds(SLEEP_INTERVAL);
            }

            // Remember start time
            start_time = chVTGetSystemTime();
        }

        // Take shot
        cameraShoot();

        // Write log
        chprintf(out, "Step: %d, time: %d, axis1: %d\r\n", i, (int)(cur_time * 1000), (int)(cur_axis0 * 1000));
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
