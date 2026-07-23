#ifndef CMD_UPTIME_H
#define CMD_UPTIME_H

#include <stdint.h>

// Вызвать один раз из kmain после rtc_init()
void uptime_init();

// Вывести uptime в формате:  up [Xd] HH:MM:SS  (now HH:MM:SS)
void cmd_uptime();

#endif