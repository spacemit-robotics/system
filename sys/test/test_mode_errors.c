/* SPDX-License-Identifier: Apache-2.0 */
/* Build with --wrap=open,write,close,ioctl,system; no hardware is accessed. */
#include <sys.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *fault;
static int operations;
static int watchdog_closes;
static int hook_calls;
static char network_command[256];

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "Failed: %s (line %d)\n", #expr, __LINE__); \
        exit(1); \
    } \
} while (0)

static int is_fault(const char *name)
{
    return strcmp(fault, name) == 0;
}

int __wrap_open(const char *path, int flags, ...)
{
    (void)flags;
    operations++;
    if (strstr(path, "/cpufreq/scaling_governor")) {
        if (is_fault("governor-open")) {
            errno = EROFS;
            return -1;
        }
        return 100;
    }
    if (strstr(path, "/backlight/")) {
        if (is_fault("backlight-missing") || is_fault("optional-missing")) {
            errno = ENOENT;
            return -1;
        }
        if (is_fault("backlight-no-device")) {
            errno = ENODEV;
            return -1;
        }
        if (is_fault("backlight-denied")) {
            errno = EACCES;
            return -1;
        }
        return 200;
    }
    if (strcmp(path, "/dev/watchdog") == 0) {
        if (is_fault("watchdog-missing") || is_fault("optional-missing")) {
            errno = ENOENT;
            return -1;
        }
        if (is_fault("watchdog-no-device")) {
            errno = ENODEV;
            return -1;
        }
        if (is_fault("watchdog-denied")) {
            errno = EACCES;
            return -1;
        }
        return 300;
    }
    if (strcmp(path, "/sys/power/state") == 0) {
        if (is_fault("suspend-open")) {
            errno = EACCES;
            return -1;
        }
        return 400;
    }
    fprintf(stderr, "Unexpected hardware path: %s\n", path);
    exit(1);
}

ssize_t __wrap_write(int fd, const void *data, size_t count)
{
    (void)data;
    operations++;
    if ((fd == 100 && is_fault("governor-write")) ||
        (fd == 200 && is_fault("backlight-write")) ||
        (fd == 300 && is_fault("watchdog-write")) ||
        (fd == 400 && is_fault("suspend-write"))) {
        errno = EIO;
        return -1;
    }
    if ((fd == 100 && is_fault("governor-short")) ||
        (fd == 200 && is_fault("backlight-short")) ||
        (fd == 300 && is_fault("watchdog-short")) ||
        (fd == 400 && is_fault("suspend-short"))) {
        return (ssize_t)count - 1;
    }
    return (ssize_t)count;
}

int __wrap_close(int fd)
{
    if (fd == 300)
        watchdog_closes++;
    errno = EBADF; /* Check that callers preserve the actual operation error. */
    return 0;
}

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    (void)request;
    REQUIRE(fd == 300);
    if (is_fault("watchdog-ioctl")) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int __wrap_system(const char *command)
{
    operations++;
    REQUIRE(strncmp(command, "ip link set ", 12) == 0);
    snprintf(network_command, sizeof(network_command), "%s", command);
    if (strncmp(fault, "network-", 8) == 0 && !is_fault("network-spawn"))
        return 256;
    if (is_fault("network-spawn")) {
        errno = ECHILD;
        return -1;
    }
    return 0;
}

static int block_hook(enum sys_mode mode, void *ctx)
{
    REQUIRE(mode == SYS_MODE_IDLE);
    REQUIRE(ctx == &hook_calls);
    hook_calls++;
    return -7;
}

int main(int argc, char **argv)
{
    REQUIRE(argc == 2);
    fault = argv[1];
    if (is_fault("hooks")) {
        sys_register_hook(block_hook, &hook_calls);
        REQUIRE(sys_set_mode(SYS_MODE_IDLE) == -7);
        REQUIRE(hook_calls == 1 && operations == 0);
        REQUIRE(sys_get_mode() == SYS_MODE_ACTIVE);
    } else if (is_fault("invalid")) {
        REQUIRE(sys_set_mode((enum sys_mode)999) < 0);
        REQUIRE(operations == 0 && sys_get_mode() == SYS_MODE_ACTIVE);
    } else if (is_fault("retry")) {
        REQUIRE(sys_set_mode(SYS_MODE_SENSOR_OFF) == 0);
        fault = "network-exit";
        REQUIRE(sys_set_mode(SYS_MODE_IDLE) < 0);
        REQUIRE(sys_get_mode() == SYS_MODE_SENSOR_OFF);
        fault = "none";
        REQUIRE(sys_set_mode(SYS_MODE_ACTIVE) == 0);
        REQUIRE(sys_get_mode() == SYS_MODE_ACTIVE);
    } else if (is_fault("success") || strstr(fault, "missing") ||
                strstr(fault, "no-device")) {
        REQUIRE(sys_set_mode(SYS_MODE_IDLE) == 0);
        REQUIRE(sys_get_mode() == SYS_MODE_IDLE);
        REQUIRE(sys_set_mode(SYS_MODE_ACTIVE) == 0);
        REQUIRE(sys_get_mode() == SYS_MODE_ACTIVE);
        const char *expected = getenv("EXPECTED_NETWORK_DEVICE");
        if (expected)
            REQUIRE(strstr(network_command, expected) != NULL);
    } else {
        enum sys_mode mode = strncmp(fault, "suspend-", 8) == 0 ?
            SYS_MODE_SUSPEND : SYS_MODE_IDLE;
        if (is_fault("network-active"))
            mode = SYS_MODE_ACTIVE;
        if (is_fault("network-sensor-off"))
            mode = SYS_MODE_SENSOR_OFF;
        if (is_fault("network-suspend"))
            mode = SYS_MODE_SUSPEND;
        if (is_fault("network-hibernate"))
            mode = SYS_MODE_HIBERNATE;
        if (is_fault("network-poweroff"))
            mode = SYS_MODE_POWEROFF;
        if (is_fault("network-reboot"))
            mode = SYS_MODE_REBOOT;
        REQUIRE(sys_set_mode(mode) < 0);
        REQUIRE(sys_get_mode() == SYS_MODE_ACTIVE);
        if (is_fault("watchdog-ioctl")) {
            REQUIRE(watchdog_closes == 1 && errno == EINVAL);
            fault = "none";
            REQUIRE(sys_set_mode(SYS_MODE_IDLE) == 0);
        }
        if (strstr(fault, "short") || strstr(fault, "-write"))
            REQUIRE(errno == EIO);
    }
    puts("PASS");
    return 0;
}
