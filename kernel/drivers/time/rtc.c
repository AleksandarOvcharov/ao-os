#include "rtc.h"
#include "io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static int cmos_updating(void) {
    outb(CMOS_ADDR, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

void rtc_read(rtc_time_t* time) {
    // Wait until CMOS is not updating
    while (cmos_updating());

    uint8_t sec  = cmos_read(0x00);
    uint8_t min  = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day  = cmos_read(0x07);
    uint8_t mon  = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);
    uint8_t wday = cmos_read(0x06);
    uint8_t regb = cmos_read(0x0B);

    // Convert BCD to binary if needed
    if (!(regb & 0x04)) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour & 0x7F);
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        year = bcd_to_bin(year);
        wday = bcd_to_bin(wday);
    }

    // Handle 12-hour format
    if (!(regb & 0x02) && (cmos_read(0x04) & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    time->second  = sec;
    time->minute  = min;
    time->hour    = hour;
    time->day     = day;
    time->month   = mon;
    time->year    = 2000 + year;
    time->weekday = wday;
}
