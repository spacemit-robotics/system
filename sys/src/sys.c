/*
    * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
    * SPDX-License-Identifier: Apache-2.0
    */
#include <sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

/* --- Macro definitions --- */
#define MAX_SYS_HOOKS 100
#define WATCHDOG_DEVICE "/dev/watchdog"
#define WATCHDOG_TIMEOUT 30  /* seconds */
#define WAKEUP_SYSFS_PATH "/sys/class/net/%s/device/power/wakeup"
#define DEFAULT_NET_DEVICE "eth0"
#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO_UNEXPORT_PATH "/sys/class/gpio/unexport"
#define GPIO_CLASS_PATH "/sys/class/gpio"
#define GPIO_DIRECTION_SUFFIX "/direction"
#define GPIO_EDGE_SUFFIX "/edge"
#define GPIO_DIRECTION_IN "in"
#define GPIO_EDGE_RISING "rising"
#define GPIO_EDGE_FALLING "falling"
#define GPIO_PATH_MAX PATH_MAX
#define GPIO_EXPORT_STR_MAX 16
#define THERMAL_CLASS_PATH "/sys/class/thermal"
#define THERMAL_ZONE_PREFIX "thermal_zone"
#define THERMAL_ZONE_TYPE_PREFIX "cluster"
#define THERMAL_VALUE_MAX 64
#define IIO_DEVICE_CLASS_PATH "/sys/bus/iio/devices"
#define IIO_DEVICE_PREFIX "iio:device"
#define IIO_SCALE_FILE_MAX 64

// 常数定义
#define ADC_REF 1.8          // ADC参考电压 (V)
#define R_REF   10.0         // 分压电阻 (kΩ) = 10
#define B_CONST 3380.0       // NTC B常数
#define T0      298.15       // 标称温度 (K) = 25°C
#define ADC_SCALE 0.7326     // ADC读数到电压的系数 (mV/计数)

/* System control paths */
#define CPU_FREQ_SCALING_GOVERNOR "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor"
#define CPU_ONLINE_PATH "/sys/devices/system/cpu/cpu%d/online"
#define BACKLIGHT_BRIGHTNESS_PATH "/sys/class/backlight/backlight/brightness"
#define BACKLIGHT_MAX_BRIGHTNESS_PATH "/sys/class/backlight/backlight/max_brightness"
#define PM_STATE_PATH "/sys/power/state"
#define PM_DISK_PATH "/sys/power/disk"
#define REBOOT_CMD "reboot"
#define POWEROFF_CMD "poweroff"

/* --- Static variables --- */
static enum sys_mode current_mode = SYS_MODE_ACTIVE;
static sys_hook_t registered_hooks[MAX_SYS_HOOKS];
static void *hook_contexts[MAX_SYS_HOOKS];
static int hook_count = 0;
static int watchdog_fd = -1;
static int watchdog_initialized = 0;

/* --- System control functions --- */

static int read_text_file(const char *path, char *buffer, size_t buffer_size)
{
    int fd;
    ssize_t bytes_read;

    if (path == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    bytes_read = read(fd, buffer, buffer_size - 1);
    close(fd);
    if (bytes_read <= 0) {
        return -1;
    }

    buffer[bytes_read] = '\0';

    while (bytes_read > 0 &&
            (buffer[bytes_read - 1] == '\n' || buffer[bytes_read - 1] == '\r')) {
        buffer[bytes_read - 1] = '\0';
        bytes_read--;
    }

    return 0;
}


/**
    * Initialize watchdog device
    * @return: 0 on success, negative on error
    */
static int watchdog_init(void)
{
    int timeout = WATCHDOG_TIMEOUT;

    if (watchdog_initialized) {
        return 0;
    }

    /* Open watchdog device */
    watchdog_fd = open(WATCHDOG_DEVICE, O_WRONLY);
    if (watchdog_fd < 0) {
        fprintf(stderr, "Failed to open watchdog device: %s\n", WATCHDOG_DEVICE);
        return -1;
    }

    /* Set watchdog timeout */
    if (ioctl(watchdog_fd, WDIOC_SETTIMEOUT, &timeout) < 0) {
        fprintf(stderr, "Failed to set watchdog timeout\n");
        close(watchdog_fd);
        watchdog_fd = -1;
        return -1;
    }

    printf("Watchdog initialized with timeout: %d seconds\n", timeout);
    watchdog_initialized = 1;

    return 0;
}

/* --- Internal mode implementation functions --- */

/**
    * Set CPU frequency scaling governor
    * @param governor: Governor name (e.g., "powersave", "performance")
    * @return: 0 on success, negative on error
    */
static int set_cpu_governor(const char *governor)
{
    char path[256];
    int fd;
    int cpu_count = 4;  /* Default to 4 CPUs */

    if (!governor) {
        fprintf(stderr, "Invalid governor name\n");
        return -1;
    }

    for (int i = 0; i < cpu_count; i++) {
        snprintf(path, sizeof(path), CPU_FREQ_SCALING_GOVERNOR, i);
        fd = open(path, O_WRONLY);
        if (fd >= 0) {
            if (write(fd, governor, strlen(governor)) < 0) {
                fprintf(stderr, "Failed to set governor for CPU %d\n", i);
            }
            close(fd);
        }
    }

    printf("CPU governor set to: %s\n", governor);
    return 0;
}

/**
    * Control screen backlight
    * @param brightness: Brightness level (0-100)
    * @return: 0 on success, negative on error
    */
static int set_backlight(int brightness)
{
    int fd;
    char value[16];

    if (brightness < 0 || brightness > 100) {
        fprintf(stderr, "Invalid brightness level: %d\n", brightness);
        return -1;
    }

    fd = open(BACKLIGHT_BRIGHTNESS_PATH, O_WRONLY);
    if (fd < 0) {
        printf("Backlight control not available\n");
        return 0;  /* Not critical */
    }

    snprintf(value, sizeof(value), "%d", brightness);
    if (write(fd, value, strlen(value)) < 0) {
        fprintf(stderr, "Failed to set backlight\n");
        close(fd);
        return -1;
    }

    close(fd);
    printf("Backlight set to: %d%%\n", brightness);
    return 0;
}

/**
    * Enable/disable network interface
    * @param enable: true to enable, false to disable
    * @return: 0 on success, negative on error
    */
static int control_network(bool enable)
{
    char cmd[256];
    int ret;

    snprintf(cmd, sizeof(cmd), "ip link set %s %s 2>/dev/null",
                DEFAULT_NET_DEVICE, enable ? "up" : "down");

    ret = system(cmd);
    if (ret == 0) {
        printf("Network %s\n", enable ? "enabled" : "disabled");
    } else {
        printf("Network control may require root privileges\n");
    }

    return 0;
}

/**
    * Suspend to RAM
    * @return: 0 on success, negative on error
    */
static int suspend_to_ram(void)
{
    int fd;
    const char *state = "mem";

    printf("Suspending to RAM...\n");

    fd = open(PM_STATE_PATH, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open PM state path\n");
        return -1;
    }

    if (write(fd, state, strlen(state)) < 0) {
        fprintf(stderr, "Failed to suspend to RAM\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/**
    * Suspend to disk (hibernation)
    * @return: 0 on success, negative on error
    */
static int suspend_to_disk(void)
{
    int fd;
    const char *state = "disk";

    printf("Suspending to disk (hibernation)...\n");

    fd = open(PM_STATE_PATH, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open PM state path\n");
        return -1;
    }

    if (write(fd, state, strlen(state)) < 0) {
        fprintf(stderr, "Failed to suspend to disk\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/**
    * Power off the system
    * @return: Does not return on success
    */
static int power_off(void)
{
    printf("Powering off system...\n");
    fflush(stdout);

    /* Try sysfs first */
    int fd = open(PM_STATE_PATH, O_WRONLY);
    if (fd >= 0) {
        write(fd, "poweroff", 8);
        close(fd);
    }

    /* Fallback to system command */
    system(POWEROFF_CMD);

    return -1;  /* Should not reach here */
}

/**
    * Reboot the system
    * @return: Does not return on success
    */
static int reboot_system(void)
{
    printf("Rebooting system...\n");
    fflush(stdout);

    /* Use system reboot command */
    system(REBOOT_CMD);

    return -1;  /* Should not reach here */
}

/**
    * Enter ACTIVE mode (full speed, all peripherals on)
    * @return: 0 on success, negative on error
    */
static int mode_active(void)
{
    printf("Entering ACTIVE mode\n");

    /* Set CPU to performance mode */
    set_cpu_governor("performance");

    /* Set backlight to full brightness */
    set_backlight(100);

    /* Enable network */
    control_network(true);

    /* Feed watchdog */
    sys_watchdog_feed();

    printf("ACTIVE mode activated\n");
    return 0;
}

/**
    * Enter IDLE mode (CPU throttled, screen off, network on)
    * @return: 0 on success, negative on error
    */
static int mode_idle(void)
{
    printf("Entering IDLE mode\n");

    /* Set CPU to powersave mode */
    set_cpu_governor("powersave");

    /* Turn off backlight */
    set_backlight(0);

    /* Keep network on */
    control_network(true);

    /* Feed watchdog */
    sys_watchdog_feed();

    printf("IDLE mode activated\n");
    return 0;
}

/**
    * Enter SENSOR_OFF mode (sensors powered off, CPU and network on)
    * @return: 0 on success, negative on error
    */
static int mode_sensor_off(void)
{
    printf("Entering SENSOR_OFF mode\n");

    /* Set CPU to powersave mode */
    set_cpu_governor("powersave");

    /* Turn off backlight */
    set_backlight(0);

    /* Keep network on */
    control_network(true);

    /* TODO: Disable sensors (IMU, camera, etc.) */
    printf("Sensors disabled\n");

    /* Feed watchdog */
    sys_watchdog_feed();

    printf("SENSOR_OFF mode activated\n");
    return 0;
}

/**
    * Enter SUSPEND mode (suspend to RAM)
    * @return: 0 on success, negative on error
    */
static int mode_suspend(void)
{
    printf("Entering SUSPEND mode\n");

    /* Disable network before suspend */
    control_network(false);

    /* Suspend to RAM */
    return suspend_to_ram();
}

/**
    * Enter HIBERNATE mode (suspend to disk)
    * @return: 0 on success, negative on error
    */
static int mode_hibernate(void)
{
    printf("Entering HIBERNATE mode\n");

    /* Disable network before hibernation */
    control_network(false);

    /* Suspend to disk */
    return suspend_to_disk();
}

/**
    * Enter POWEROFF mode
    * @return: Does not return on success
    */
static int mode_poweroff(void)
{
    printf("Entering POWEROFF mode\n");

    /* Disable network */
    control_network(false);

    /* Power off */
    return power_off();
}

/**
    * Enter REBOOT mode
    * @return: Does not return on success
    */
static int mode_reboot(void)
{
    printf("Entering REBOOT mode\n");

    /* Disable network */
    control_network(false);

    /* Reboot */
    return reboot_system();
}

/**
    * Execute mode-specific operations
    * @param mode: Target system mode
    * @return: 0 on success, negative on error
    */
static int execute_mode_operations(enum sys_mode mode)
{
    switch (mode) {
    case SYS_MODE_ACTIVE:
        return mode_active();
    case SYS_MODE_IDLE:
        return mode_idle();
    case SYS_MODE_SENSOR_OFF:
        return mode_sensor_off();
    case SYS_MODE_SUSPEND:
        return mode_suspend();
    case SYS_MODE_HIBERNATE:
        return mode_hibernate();
    case SYS_MODE_POWEROFF:
        return mode_poweroff();
    case SYS_MODE_REBOOT:
        return mode_reboot();
    default:
        fprintf(stderr, "Unknown system mode: %d\n", mode);
        return -1;
    }
}

/**
    * Feed the watchdog timer
    */
void sys_watchdog_feed(void)
{
    int ret;

    /* Initialize watchdog on first call */
    if (!watchdog_initialized) {
        if (watchdog_init() < 0) {
            fprintf(stderr, "Watchdog initialization failed\n");
            return;
        }
    }

    /* Feed the watchdog by writing to device */
    if (watchdog_fd >= 0) {
        ret = write(watchdog_fd, "1", 1);
        if (ret < 0) {
            fprintf(stderr, "Failed to feed watchdog\n");
        } else {
            printf("Watchdog fed successfully\n");
        }
    }
}

int sys_set_mode(enum sys_mode mode)
{
    int ret = 0;

    /* Validate mode */
    if (mode < SYS_MODE_ACTIVE || mode > SYS_MODE_REBOOT) {
        fprintf(stderr, "Invalid system mode: %d\n", mode);
        return -1;
    }

    /* Call registered hooks before mode change */
    for (int i = 0; i < hook_count; i++) {
        if (registered_hooks[i]) {
            ret = registered_hooks[i](mode, hook_contexts[i]);
            if (ret != 0) {
                fprintf(stderr, "Hook %d failed with code %d\n", i, ret);
                return ret;
            }
        }
    }

    /* Execute mode-specific operations */
    ret = execute_mode_operations(mode);
    if (ret != 0) {
        fprintf(stderr, "Failed to execute mode operations\n");
        return ret;
    }

    /* Update current mode */
    current_mode = mode;

    printf("System mode changed to: %d\n", mode);

    return 0;
}

/**
    * Get current system power mode
    * @return: Current system mode
    */
enum sys_mode sys_get_mode(void)
{
    return current_mode;
}

/**
    * Register a hook function to be called on system mode changes
    * @param hook: Hook function pointer
    * @param ctx: Context pointer to pass to hook
    */
void sys_register_hook(sys_hook_t hook, void *ctx)
{
    if (hook_count >= MAX_SYS_HOOKS) {
        fprintf(stderr, "Maximum number of hooks reached (%d)\n", MAX_SYS_HOOKS);
        return;
    }

    if (hook == NULL) {
        fprintf(stderr, "Hook function pointer is NULL\n");
        return;
    }

    registered_hooks[hook_count] = hook;
    hook_contexts[hook_count] = ctx;
    hook_count++;

    printf("Hook registered (total: %d)\n", hook_count);
}


int sys_get_cpu_temperature(float *temp)
{
    DIR *thermal_dir;
    struct dirent *entry;
    char type_path[GPIO_PATH_MAX];
    char temp_path[GPIO_PATH_MAX];
    char type_value[THERMAL_VALUE_MAX];
    char temp_value[THERMAL_VALUE_MAX];
    char *endptr;
    int64_t raw_temp;

    if (temp == NULL) {
        fprintf(stderr, "Temperature output pointer is NULL\n");
        return -1;
    }

    thermal_dir = opendir(THERMAL_CLASS_PATH);
    if (thermal_dir == NULL) {
        fprintf(stderr, "Failed to open thermal class path: %s\n", THERMAL_CLASS_PATH);
        return -1;
    }

    while ((entry = readdir(thermal_dir)) != NULL) {
        if (strncmp(entry->d_name, THERMAL_ZONE_PREFIX, strlen(THERMAL_ZONE_PREFIX)) != 0) {
            continue;
        }

        snprintf(type_path, sizeof(type_path), "%s/%s/type", THERMAL_CLASS_PATH, entry->d_name);
        if (read_text_file(type_path, type_value, sizeof(type_value)) < 0) {
            continue;
        }

        if (strncmp(type_value, THERMAL_ZONE_TYPE_PREFIX, strlen(THERMAL_ZONE_TYPE_PREFIX)) != 0) {
            continue;
        }

        snprintf(temp_path, sizeof(temp_path), "%s/%s/temp", THERMAL_CLASS_PATH, entry->d_name);
        if (read_text_file(temp_path, temp_value, sizeof(temp_value)) < 0) {
            fprintf(stderr, "Failed to read temperature from %s\n", temp_path);
            closedir(thermal_dir);
            return -1;
        }

        errno = 0;
        raw_temp = strtoll(temp_value, &endptr, 10);
        if (errno != 0 || endptr == temp_value) {
            fprintf(stderr, "Invalid temperature value in %s\n", temp_path);
            closedir(thermal_dir);
            return -1;
        }

        *temp = (float)raw_temp / 1000.0f;
        closedir(thermal_dir);
        return 0;
    }

    closedir(thermal_dir);
    fprintf(stderr, "No CPU thermal zone with type prefix %s found\n", THERMAL_ZONE_TYPE_PREFIX);
    return -1;
}

int sys_get_adc_value(int channel, int *value)
{
    DIR *iio_dir;
    struct dirent *entry;
    char raw_name[IIO_SCALE_FILE_MAX];
    char raw_path[GPIO_PATH_MAX];
    char raw_value_text[THERMAL_VALUE_MAX];
    char *endptr;
    double raw_value;

    if (channel < 0) {
        fprintf(stderr, "Invalid ADC channel: %d\n", channel);
        return -1;
    }

    if (value == NULL) {
        fprintf(stderr, "ADC output pointer is NULL\n");
        return -1;
    }

    snprintf(raw_name, sizeof(raw_name), "in_voltage%d_raw", channel);

    iio_dir = opendir(IIO_DEVICE_CLASS_PATH);
    if (iio_dir == NULL) {
        fprintf(stderr, "Failed to open IIO device path: %s\n", IIO_DEVICE_CLASS_PATH);
        return -1;
    }

    while ((entry = readdir(iio_dir)) != NULL) {
        if (strncmp(entry->d_name, IIO_DEVICE_PREFIX, strlen(IIO_DEVICE_PREFIX)) != 0) {
            continue;
        }

        snprintf(raw_path, sizeof(raw_path), "%s/%s/%s",
                IIO_DEVICE_CLASS_PATH, entry->d_name, raw_name);
        if (read_text_file(raw_path, raw_value_text, sizeof(raw_value_text)) < 0) {
            continue;
        }

        errno = 0;
        raw_value = strtod(raw_value_text, &endptr);
        if (errno != 0 || endptr == raw_value_text) {
            fprintf(stderr, "Invalid ADC raw value in %s\n", raw_path);
            closedir(iio_dir);
            return -1;
        }

        *value = (int)raw_value;
        closedir(iio_dir);
        return 0;
    }

    closedir(iio_dir);
    fprintf(stderr, "No ADC raw node %s found\n", raw_name);
    return -1;
}

int sys_get_ntc_temperature(int channel, float *temp)
{
    int adc_value = 0;
    double voltage;
    double v_ratio;
    double resistance;
    double ln_resistance;
    double inv_t;
    double temp_kelvin;
    double temp_celsius;

    if (temp == NULL) {
        fprintf(stderr, "NTC output pointer is NULL\n");
        return -1;
    }

    if (sys_get_adc_value(channel, &adc_value) != 0) {
        return -1;
    }

    /* Convert ADC reading to voltage in volts. */
    voltage = adc_value * ADC_SCALE / 1000.0;
    if (voltage >= ADC_REF) {
        return -1;
    }

    v_ratio = voltage / (ADC_REF - voltage);
    resistance = v_ratio / R_REF;  /* kOhm */
    ln_resistance = log(resistance);
    inv_t = (1.0 / T0) + (1.0 / B_CONST) * ln_resistance;
    temp_kelvin = 1.0 / inv_t;
    temp_celsius = temp_kelvin - 273.15;
    *temp = (float)temp_celsius;
    return 0;
}

/* --- Wakeup source control --- */

/**
    * Enable/disable WoL (Wake-on-LAN) via ethtool
    * @param ifname: Network interface name (e.g., "eth0")
    * @param enable: true to enable, false to disable
    * @return: 0 on success, negative on error
    */
static int wol_control(const char *ifname, bool enable)
{
    int sock;
    struct ifreq ifr;
    struct ethtool_wolinfo wol;

    if (!ifname) {
        fprintf(stderr, "Invalid interface name\n");
        return -1;
    }

    /* Create socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return -1;
    }

    /* Prepare interface request */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    /* Prepare WoL settings */
    memset(&wol, 0, sizeof(wol));
    wol.cmd = ETHTOOL_SWOL;

    if (enable) {
        wol.wolopts = WAKE_MAGIC;  /* Enable magic packet wake-up */
    } else {
        wol.wolopts = 0;  /* Disable all wake-up options */
    }

    ifr.ifr_data = (caddr_t)&wol;

    /* Apply WoL settings */
    if (ioctl(sock, SIOCETHTOOL, &ifr) < 0) {
        fprintf(stderr, "Failed to set WoL for %s: %m\n", ifname);
        close(sock);
        return -1;
    }

    close(sock);
    printf("WoL %s for interface %s\n", enable ? "enabled" : "disabled", ifname);

    return 0;
}

/**
    * Enable/disable network as wakeup source
    * @param enable: true to enable, false to disable
    * @return: 0 on success, negative on error
    */
int sys_enable_wakeup_net(bool enable)
{
    int ret;
    char sysfs_path[GPIO_PATH_MAX];
    int fd;
    const char *value;

    /* Try to enable WoL via ethtool */
    ret = wol_control(DEFAULT_NET_DEVICE, enable);
    if (ret < 0) {
        fprintf(stderr, "WoL control failed\n");
        return ret;
    }

    /* Also try to enable via sysfs wakeup attribute */
    snprintf(sysfs_path, sizeof(sysfs_path), WAKEUP_SYSFS_PATH, DEFAULT_NET_DEVICE);

    fd = open(sysfs_path, O_WRONLY);
    if (fd >= 0) {
        value = enable ? "enabled" : "disabled";
        if (write(fd, value, strlen(value)) < 0) {
            fprintf(stderr, "Failed to write to %s\n", sysfs_path);
        } else {
            printf("Network wakeup source %s via sysfs\n", value);
        }
        close(fd);
    } else {
        printf("Wakeup sysfs path not available: %s\n", sysfs_path);
    }

    return 0;
}

/**
    * Enable/disable GPIO pin as wakeup source
    * @param pin: GPIO pin number
    * @param active_low: true if active low, false if active high
    * @return: 0 on success, negative on error
    */
int sys_enable_wakeup_gpio(int pin, bool active_low)
{
    char gpio_path[GPIO_PATH_MAX];
    char edge_path[GPIO_PATH_MAX];
    char direction_path[GPIO_PATH_MAX];
    int fd;
    const char *edge_value;

    if (pin < 0) {
        fprintf(stderr, "Invalid GPIO pin: %d\n", pin);
        return -1;
    }

    /* Export GPIO pin if not already exported */
    fd = open(GPIO_EXPORT_PATH, O_WRONLY);
    if (fd >= 0) {
        char export_str[GPIO_EXPORT_STR_MAX];
        snprintf(export_str, sizeof(export_str), "%d", pin);
        if (write(fd, export_str, strlen(export_str)) < 0) {
            /* Pin might already be exported, ignore error */
        }
        close(fd);
    }

    /* Set GPIO direction to input */
    snprintf(direction_path, sizeof(direction_path), "%s/gpio%d%s",
                GPIO_CLASS_PATH, pin, GPIO_DIRECTION_SUFFIX);
    fd = open(direction_path, O_WRONLY);
    if (fd >= 0) {
        if (write(fd, GPIO_DIRECTION_IN, strlen(GPIO_DIRECTION_IN)) < 0) {
            fprintf(stderr, "Failed to set GPIO %d direction\n", pin);
            close(fd);
            return -1;
        }
        close(fd);
    } else {
        fprintf(stderr, "Failed to open GPIO %d direction\n", pin);
        return -1;
    }

    /* Set GPIO edge trigger */
    snprintf(edge_path, sizeof(edge_path), "%s/gpio%d%s",
                GPIO_CLASS_PATH, pin, GPIO_EDGE_SUFFIX);
    fd = open(edge_path, O_WRONLY);
    if (fd >= 0) {
        edge_value = active_low ? GPIO_EDGE_FALLING : GPIO_EDGE_RISING;
        if (write(fd, edge_value, strlen(edge_value)) < 0) {
            fprintf(stderr, "Failed to set GPIO %d edge\n", pin);
            close(fd);
            return -1;
        }
        close(fd);
    } else {
        fprintf(stderr, "Failed to open GPIO %d edge\n", pin);
        return -1;
    }

    printf("GPIO pin %d wakeup source enabled (active_%s)\n",
            pin, active_low ? "low" : "high");

    return 0;
}
