#ifndef RTC_H
#define RTC_H

#include <stdint.h>

// RTC structure
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} RTC_Time;

// Functions
void rtc_init();
RTC_Time rtc_read_time();
void rtc_get_time_string(char* buffer); // Format: "HH:MM:SS"

#endif
