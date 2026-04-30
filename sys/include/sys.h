/*
    * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
    * SPDX-License-Identifier: Apache-2.0
    */
#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    * System power modes
    */
enum sys_mode {
    SYS_MODE_ACTIVE = 0,   /* full speed, all peripherals on */
    SYS_MODE_IDLE,         /* CPU throttled, screen off, network on */
    SYS_MODE_SENSOR_OFF,   /* sensors powered off, CPU and network on */
    SYS_MODE_SUSPEND,      /* suspend to RAM */
    SYS_MODE_HIBERNATE,    /* suspend to disk */
    SYS_MODE_POWEROFF,
    SYS_MODE_REBOOT,
};

/* --- system control --- */

int sys_set_mode(enum sys_mode mode);
enum sys_mode sys_get_mode(void);

typedef int (*sys_hook_t)(enum sys_mode target_mode, void *ctx);
void sys_register_hook(sys_hook_t hook, void *ctx);

void sys_watchdog_feed(void);

/* --- wakeup source control --- */

int sys_enable_wakeup_net(bool enable);
int sys_enable_wakeup_gpio(int pin, bool active_low);

/*sys info*/
int sys_get_cpu_temperature(float *temp);
int sys_get_adc_value(int channel, int *value);
int sys_get_ntc_temperature(int channel, float *temp);


#ifdef __cplusplus
}
#endif

#endif /* SYS_H */
