#include "rtc.h"
#include "../kernel/screen.h"

// Port definitions
#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

// CMOS register addresses
#define RTC_SECONDS  0x00
#define RTC_MINUTES  0x02
#define RTC_HOURS    0x04
#define RTC_DAY      0x07
#define RTC_MONTH    0x08
#define RTC_YEAR     0x09
#define RTC_REGISTER_A 0x0A
#define RTC_REGISTER_B 0x0B

extern "C" uint8_t inb(uint16_t port);
extern "C" void outb(uint16_t port, uint8_t val);

// Read from CMOS
static uint8_t rtc_read_register(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

// Convert BCD to binary
static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd / 16) * 10) + (bcd & 0x0F);
}

// Check if RTC is updating
static int rtc_is_updating() {
    outb(CMOS_ADDRESS, RTC_REGISTER_A);
    return (inb(CMOS_DATA) & 0x80);
}

void rtc_init() {
    // Enable RTC
    outb(CMOS_ADDRESS, RTC_REGISTER_B);
    uint8_t prev = inb(CMOS_DATA);
    outb(CMOS_ADDRESS, RTC_REGISTER_B);
    outb(CMOS_DATA, prev | 0x02); // 24-hour mode
}

RTC_Time rtc_read_time() {
    RTC_Time time;
    
    // Wait until RTC is not updating
    while (rtc_is_updating());
    
    uint8_t second = rtc_read_register(RTC_SECONDS);
    uint8_t minute = rtc_read_register(RTC_MINUTES);
    uint8_t hour = rtc_read_register(RTC_HOURS);
    uint8_t day = rtc_read_register(RTC_DAY);
    uint8_t month = rtc_read_register(RTC_MONTH);
    uint8_t year = rtc_read_register(RTC_YEAR);
    
    // Check if BCD format (bit 2 of Register B)
    outb(CMOS_ADDRESS, RTC_REGISTER_B);
    uint8_t registerB = inb(CMOS_DATA);
    
    if (!(registerB & 0x04)) {
        // BCD mode - convert to binary
        time.second = bcd_to_binary(second);
        time.minute = bcd_to_binary(minute);
        time.hour = bcd_to_binary(hour);
        time.day = bcd_to_binary(day);
        time.month = bcd_to_binary(month);
        time.year = bcd_to_binary(year);
    } else {
        // Binary mode
        time.second = second;
        time.minute = minute;
        time.hour = hour;
        time.day = day;
        time.month = month;
        time.year = year;
    }
    
    time.year += 2000; // Assume 21st century
    
    return time;
}

void rtc_get_time_string(char* buffer) {
    RTC_Time time = rtc_read_time();
    
    // Format: "HH:MM:SS"
    buffer[0] = '0' + (time.hour / 10);
    buffer[1] = '0' + (time.hour % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (time.minute / 10);
    buffer[4] = '0' + (time.minute % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (time.second / 10);
    buffer[7] = '0' + (time.second % 10);
    buffer[8] = '\0';
}
