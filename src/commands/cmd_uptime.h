#ifndef CMD_UPTIME_H
#define CMD_UPTIME_H

extern "C" {
    void cmd_uptime_tick();   // вызывать из главного цикла каждую итерацию
    void cmd_uptime();        // вывести uptime
}

#endif