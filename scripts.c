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

// todo get moto speed, scale
#define TICKS_PER_SECOND        1000    // ticks
#define SLEEP_INTERVAL          10      // ticks

#define SHOT_TIME               0.3     // sec
#define STABILIZATION_TIME      0.1     // sec

/*
 * For recursion
 */
float parse_add(char **str, float n);
float parse_group(char **str, float n);

/*
 * Solver main function
 */
float solve_expr(char *expr, float n)
{
    char *expr_copy = expr;
    return parse_add(&expr_copy, n);
}

/*
 * Check script for time interval correctness
 */
int scripts_check(char *script, BaseSequentialStream *out)
{
    chprintf(out, "Checking script...\r\n");

    // Local copy(split corrupts string)
    char script_cpy[128];
    strcpy(script_cpy, script);

    // Splitting expression
    int shots = atoi(strtok(script_cpy, "[;]"));
    char *time_expr = strtok(NULL, "[;]");
    char *axis1_expr = strtok(NULL, "[;]");

    int i;
    for (i = 1; i < shots; i++)
    {
        // Calculating coordinates
        float prev_time  = solve_expr(time_expr, i - 1);
        float prev_axis1 = solve_expr(axis1_expr, i - 1);

        float cur_time = solve_expr(time_expr, i);
        float cur_axis1 = solve_expr(axis1_expr, i);

        float axis1_move_time = fabs(prev_axis1 - cur_axis1) * AXIS1_STEPS_PER_METER / AXIS1_STEPS_PER_SECOND;

        // Check time correctness
        if (cur_time - prev_time < axis1_move_time + SHOT_TIME + STABILIZATION_TIME)  // axis1
        {
            chprintf(out, "Axis1 cann't be in time.\r\n\r\n");

            chprintf(out, "Previous step: %d, time: %d, pos: %d\r\n", i - 1, (int)(prev_time * 1000), (int)(prev_axis1 * 1000));
            chprintf(out, "Current step: %d, time: %d, pos: %d\r\n\r\n", i, (int)(cur_time * 1000), (int)(cur_axis1 * 1000));


            chprintf(out, "Free time: %d ms\r\n", (int)((cur_time - prev_time) * 1000));
            chprintf(out, "Move time: %d ms\r\n", (int)(axis1_move_time * 1000));
            chprintf(out, "Shot time: %d ms\r\n", (int)(SHOT_TIME * 1000));
            chprintf(out, "Stabilization time: %d ms\r\n", (int)(STABILIZATION_TIME * 1000));
            chprintf(out, "Time defect: %d ms\r\n", (int)(( (axis1_move_time + SHOT_TIME + STABILIZATION_TIME) - (cur_time - prev_time) ) * 1000));

            return 1;   // Error
        }
    }

    chprintf(out, "Ok.\r\n");
    return 0;
}

/*
 * Script format: [shots;time_expr;axis1_expr] (time in seconds, location in meters)
 */
int scripts_execute(char *script, BaseSequentialStream *out)
{
    // Checking script
    if (scripts_check(script, out))
    {
        return 1;   // Error
    }

    // Local copy(split corrupts string)
    char script_cpy[128];
    strcpy(script_cpy, script);

    // Splitting expression
    int shots = atoi(strtok(script_cpy, "[;]"));
    char *time_expr = strtok(NULL, "[;]");
    char *axis1_expr = strtok(NULL, "[;]");

    systime_t start_time;

    int i;
    for (i = 0; i < shots; i++)
    {
        // Calculating coordinates
        float cur_time = solve_expr(time_expr, i);
        float cur_axis1 = solve_expr(axis1_expr, i);

        if (i)  // Not first shot
        {
            float prev_time  = solve_expr(time_expr, i - 1);

            // Wait for | shot time + free time = interval - stabilization - move
            while (chVTGetSystemTime() - start_time < (systime_t)((prev_time + SHOT_TIME) * TICKS_PER_SECOND))
            {
                chThdSleepMilliseconds(SLEEP_INTERVAL);
            }

            // Move axis
            motoGoPos(MOTO_LINE, (int)(cur_axis1 * AXIS1_STEPS_PER_METER));

            // Wait for shot
            while (chVTGetSystemTime() - start_time < (systime_t)(cur_time * TICKS_PER_SECOND))
            {
                chThdSleepMilliseconds(SLEEP_INTERVAL);
            }
        }
        else    // First shot
        {
            // Move axis to start position
            motoGoPos(MOTO_LINE, (int)(cur_axis1 * AXIS1_STEPS_PER_METER));

            // Wait for axis
            while (getMotoState(MOTO_LINE) == MOTO_STATE_RUN)
            {
                chThdSleepMilliseconds(SLEEP_INTERVAL);
            }

            // Remember start time
            start_time = chVTGetSystemTime();
        }

        // Take shot
        camera_shoot();

        // Write log
        chprintf(out, "Step: %d, time: %d, axis1: %d\r\n", i, (int)(cur_time * 1000), (int)(cur_axis1 * 1000));
    }

    return 0;
}

/*
 * primitive_value : NUMBER | ID | func_call ;
 */
float parse_primitive_value(char **str, float n)
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
            result = sin(parse_group(str, n));
            break;
        case 't':           // tan
            (*str) += 3;
            result = tan(parse_group(str, n));
            break;
        case 'c':           // cos
            (*str) += 3;
            result = cos(parse_group(str, n));
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
float parse_group(char **str, float n)
{
    float result = 0;

    if (**str == '(')
    {
        (*str)++;  // '('
        result = parse_add(str, n); // value_add
        (*str)++;  // ')'
    }
    else
    {
        result = parse_primitive_value(str, n);
    }

    return result;
}

/*
 * value_mult : value_group ( ( MUL | DIV )^ value_group )* ;
 */
float parse_mult(char **str, float n)
{
    float result = parse_group(str, n);

    while (**str == '*' || **str == '/')
    {
        switch (**str)
        {
            case '*':
                (*str)++;
                result *= parse_group(str, n);
                break;
            case '/':
                (*str)++;
                result /= parse_group(str, n);
                break;
        }
    }

    return result;
}

/*
 * value_add : value_mult ( ( ADD | SUB )^ value_mult )* ;
 */
float parse_add(char **str, float n)
{
    float result = parse_mult(str, n);

    while (**str == '+' || **str == '-')
    {
        switch (**str)
        {
            case '+':
                (*str)++;
                result += parse_mult(str, n);
                break;
            case '-':
                (*str)++;
                result -= parse_mult(str, n);
                break;
        }
    }

    return result;
}
