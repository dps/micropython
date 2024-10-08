// Credit to https://github.com/ghubcoder/micropython-pico-deepsleep

#include "py/runtime.h"
#include "pico/sleep.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"

#define STATIC static

static void sleep_callback(void)
{
    return;
}

void datetime_to_tm(const datetime_t *dt, struct tm *tm_time) {
    tm_time->tm_year = dt->year - 1900;  // struct tm year is year since 1900
    tm_time->tm_mon = dt->month - 1;     // struct tm month is 0-11
    tm_time->tm_mday = dt->day;
    tm_time->tm_hour = dt->hour;
    tm_time->tm_min = dt->min;
    tm_time->tm_sec = dt->sec;
}

static void rtc_sleep_seconds(uint32_t seconds_to_sleep)
{

    // Hangs if we attempt to sleep for 1 second....
    // Guard against this and perform a normal sleep
    if (seconds_to_sleep == 1)
    {
        sleep_ms(1000);
        return;
    }

    struct tm tm_time;
    datetime_t now;
    rtc_get_datetime(&now);
    datetime_to_tm(&now, &tm_time);
    time_t epoch = mktime(&tm_time);
    epoch += seconds_to_sleep;

    // int y = 2020, m = 6, d = 5, hour = 15, mins = 45, secs = 0;
    // struct tm t = {.tm_year = y - 1900,
    //                .tm_mon = m - 1,
    //                .tm_mday = d,
    //                .tm_hour = hour,
    //                .tm_min = mins,
    //                .tm_sec = secs};

    // t.tm_sec += seconds_to_sleep;
    // mktime(&t);

    struct tm* t = gmtime(&epoch);

    datetime_t t_alarm = {
        .year = -1, //t->tm_year + 1900,
        .month = t->tm_mon + 1,
        .day = t->tm_mday,
        .dotw = -1, //t->tm_wday, // 0 is Sunday, so 5 is Friday
        .hour = t->tm_hour,
        .min = t->tm_min,
        .sec = t->tm_sec};

    sleep_goto_sleep_until(&t_alarm, &sleep_callback);
}

void recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig)
{

    // Re-enable ring Oscillator control
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    // reset procs back to default
    scb_hw->scr = scb_orig;
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;

    // reset clocks
    clocks_init();
    // stdio_init_all();

    return;
}

STATIC mp_obj_t picosleep_pin(mp_obj_t pin_obj, mp_obj_t edge_obj, mp_obj_t high_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);
    bool edge = mp_obj_get_int(edge_obj) == 1;
    bool high = mp_obj_get_int(high_obj) == 1;
    
    uint scb_orig = scb_hw->scr;
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;

    sleep_run_from_rosc();

    sleep_goto_dormant_until_pin(pin, edge, high);
    recover_from_sleep(scb_orig, clock0_orig, clock1_orig);

    return mp_const_none;
}

STATIC mp_obj_t picosleep_seconds(mp_obj_t seconds_obj)
{
    mp_int_t seconds = mp_obj_get_int(seconds_obj);
    //stdio_init_all();
    // save values for later
    uint scb_orig = scb_hw->scr;
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;

    // // crudely reset the clock each time
    // // to the value below
    // datetime_t t = {
    //     .year = 2020,
    //     .month = 06,
    //     .day = 05,
    //     .dotw = 5, // 0 is Sunday, so 5 is Friday
    //     .hour = 15,
    //     .min = 45,
    //     .sec = 00};

    
    //sleep_run_from_xosc();
    sleep_run_from_rosc();
    // // Start the Real time clock
    if (!rtc_running()) {
        rtc_init();
    }
    // rtc_set_datetime(&t);
    rtc_sleep_seconds(seconds);
    recover_from_sleep(scb_orig, clock0_orig, clock1_orig);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(picosleep_seconds_obj, picosleep_seconds);
MP_DEFINE_CONST_FUN_OBJ_3(picosleep_pin_obj, picosleep_pin);

STATIC const mp_rom_map_elem_t picosleep_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_picosleep)},
    {MP_ROM_QSTR(MP_QSTR_seconds), MP_ROM_PTR(&picosleep_seconds_obj)},
    {MP_ROM_QSTR(MP_QSTR_pin), MP_ROM_PTR(&picosleep_pin_obj)}
};
STATIC MP_DEFINE_CONST_DICT(picosleep_module_globals, picosleep_module_globals_table);

const mp_obj_module_t mp_module_picosleep = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&picosleep_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_picosleep, mp_module_picosleep);