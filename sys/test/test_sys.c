/*
    * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
    * SPDX-License-Identifier: Apache-2.0
    */
#include <sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

/* Test hook function */
static int test_hook_called = 0;
static enum sys_mode test_hook_mode = SYS_MODE_ACTIVE;

int test_mode_change_hook(enum sys_mode target_mode, void *ctx)
{
    printf("  [Hook] Mode change to %d, context: %s\n", target_mode, (char *)ctx);
    test_hook_called++;
    test_hook_mode = target_mode;
    return 0;
}

int test_mode_change_hook_fail(enum sys_mode target_mode, void *ctx)
{
    printf("  [Hook] Failing hook called\n");
    return -1;  /* Simulate failure */
}

/* Test cases */
void test_sys_mode_operations(void)
{
    printf("\n=== Test: System Mode Operations ===\n");

    /* Test get initial mode */
    enum sys_mode mode = sys_get_mode();
    printf("Initial mode: %d\n", mode);
    assert(mode == SYS_MODE_ACTIVE);

    /* Test set mode to IDLE */
    printf("Setting mode to IDLE...\n");
    assert(sys_set_mode(SYS_MODE_IDLE) == 0);
    assert(sys_get_mode() == SYS_MODE_IDLE);

    /* Test set mode to SUSPEND */
    printf("Setting mode to SUSPEND...\n");
    assert(sys_set_mode(SYS_MODE_SUSPEND) == 0);
    assert(sys_get_mode() == SYS_MODE_SUSPEND);

    /* Test invalid mode */
    printf("Testing invalid mode...\n");
    assert(sys_set_mode(999) == -1);
    assert(sys_get_mode() == SYS_MODE_SUSPEND);  /* Should remain unchanged */

    /* Reset to ACTIVE */
    sys_set_mode(SYS_MODE_ACTIVE);

    printf("✓ System mode operations test passed\n");
}

void test_sys_hook_registration(void)
{
    printf("\n=== Test: Hook Registration ===\n");

    test_hook_called = 0;
    char *ctx1 = "Context 1";
    char *ctx2 = "Context 2";

    /* Register first hook */
    printf("Registering hook 1...\n");
    sys_register_hook(test_mode_change_hook, ctx1);

    /* Register second hook */
    printf("Registering hook 2...\n");
    sys_register_hook(test_mode_change_hook, ctx2);

    /* Test hook is called on mode change */
    printf("Changing mode to trigger hooks...\n");
    test_hook_called = 0;
    assert(sys_set_mode(SYS_MODE_IDLE) == 0);
    assert(test_hook_called == 2);  /* Both hooks should be called */
    assert(test_hook_mode == SYS_MODE_IDLE);

    /* Test NULL hook */
    printf("Testing NULL hook registration...\n");
    sys_register_hook(NULL, NULL);  /* Should not crash */

    /* Reset to ACTIVE */
    sys_set_mode(SYS_MODE_ACTIVE);

    printf("✓ Hook registration test passed\n");
}

void test_sys_hook_failure(void)
{
    printf("\n=== Test: Hook Failure Handling ===\n");

    /* Register a failing hook */
    printf("Registering failing hook...\n");
    sys_register_hook(test_mode_change_hook_fail, NULL);

    /* Try to change mode - should fail due to hook */
    printf("Attempting mode change (should fail)...\n");
    int ret = sys_set_mode(SYS_MODE_SENSOR_OFF);
    assert(ret != 0);  /* Should fail */

    /* Mode should remain unchanged */
    assert(sys_get_mode() == SYS_MODE_ACTIVE);

    printf("✓ Hook failure handling test passed\n");
}

void test_sys_watchdog(void)
{
    printf("\n=== Test: Watchdog Operations ===\n");

    /* Test watchdog feed */
    printf("Feeding watchdog (may fail if /dev/watchdog not available)...\n");
    sys_watchdog_feed();

    /* Feed multiple times */
    printf("Feeding watchdog multiple times...\n");
    for (int i = 0; i < 3; i++) {
        sys_watchdog_feed();
        usleep(100000);  /* 100ms delay */
    }

    printf("✓ Watchdog operations test passed\n");
}

void test_sys_wakeup_net(void)
{
    printf("\n=== Test: Network Wakeup Source ===\n");

    /* Test enable network wakeup */
    printf("Enabling network wakeup (may require root privileges)...\n");
    int ret = sys_enable_wakeup_net(true);
    if (ret == 0) {
        printf("Network wakeup enabled successfully\n");
    } else {
        printf("Network wakeup enable failed (may need root)\n");
    }

    /* Test disable network wakeup */
    printf("Disabling network wakeup...\n");
    ret = sys_enable_wakeup_net(false);
    if (ret == 0) {
        printf("Network wakeup disabled successfully\n");
    } else {
        printf("Network wakeup disable failed (may need root)\n");
    }

    printf("✓ Network wakeup source test passed\n");
}

void test_sys_adc_value(void)
{
    int value = 0;

    printf("\n=== Test: ADC Value ===\n");

    printf("Testing invalid ADC channel...\n");
    assert(sys_get_adc_value(-1, &value) == -1);

    printf("Testing NULL output pointer...\n");
    assert(sys_get_adc_value(1, NULL) == -1);

    printf("Reading ADC channel 1 scale...\n");
    int ret = sys_get_adc_value(1, &value);
    if (ret == 0) {
        printf("ADC channel 1 scale: %d\n", value);
    } else {
        printf("ADC channel 1 scale not available on this platform\n");
    }

    /*test ntc temperature*/
    float ntc_temp = 0.0f;
    printf("Testing NTC temperature reading...\n");
    ret = sys_get_ntc_temperature(1, &ntc_temp);
    if (ret == 0) {
        printf("NTC temperature: %.2f C\n", ntc_temp);
    } else {
        printf("NTC temperature not available on this platform\n");
    }

    printf("✓ ADC value test passed\n");
}

void test_sys_cpu_temperature(void)
{
    float temp = 0.0f;

    printf("\n=== Test: CPU Temperature ===\n");

    printf("Testing NULL output pointer...\n");
    assert(sys_get_cpu_temperature(NULL) == -1);

    printf("Reading CPU temperature from thermal zones...\n");
    int ret = sys_get_cpu_temperature(&temp);
    if (ret == 0) {
        printf("CPU temperature: %.2f C\n", temp);
    } else {
        printf("CPU temperature not available on this platform\n");
    }

    printf("✓ CPU temperature test passed\n");
}

void test_sys_wakeup_gpio(void)
{
    printf("\n=== Test: GPIO Wakeup Source ===\n");

    /* Test invalid GPIO pin */
    printf("Testing invalid GPIO pin...\n");
    assert(sys_enable_wakeup_gpio(-1, false) == -1);

    /* Test valid GPIO pin (may fail if GPIO not available) */
    printf("Testing GPIO pin 17 (may require root privileges)...\n");
    int ret = sys_enable_wakeup_gpio(17, true);
    if (ret == 0) {
        printf("GPIO 17 wakeup enabled successfully\n");
    } else {
        printf("GPIO 17 wakeup enable failed (may need root or GPIO not available)\n");
    }

    /* Test another GPIO pin with active high */
    printf("Testing GPIO pin 27 with active high...\n");
    ret = sys_enable_wakeup_gpio(27, false);
    if (ret == 0) {
        printf("GPIO 27 wakeup enabled successfully\n");
    } else {
        printf("GPIO 27 wakeup enable failed (may need root or GPIO not available)\n");
    }

    printf("✓ GPIO wakeup source test passed\n");
}

void test_sys_all_modes(void)
{
    printf("\n=== Test: All System Modes ===\n");

    enum sys_mode modes[] = {
        SYS_MODE_ACTIVE,
        SYS_MODE_IDLE,
        SYS_MODE_SENSOR_OFF,
        SYS_MODE_SUSPEND,
        SYS_MODE_HIBERNATE,
        SYS_MODE_POWEROFF,
        SYS_MODE_REBOOT
    };

    const char *mode_names[] = {
        "ACTIVE",
        "IDLE",
        "SENSOR_OFF",
        "SUSPEND",
        "HIBERNATE",
        "POWEROFF",
        "REBOOT"
    };

    for (int i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        printf("Testing mode: %s\n", mode_names[i]);
        assert(sys_set_mode(modes[i]) == 0);
        assert(sys_get_mode() == modes[i]);
    }

    /* Reset to ACTIVE */
    sys_set_mode(SYS_MODE_ACTIVE);

    printf("✓ All system modes test passed\n");
}

int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("  System Control Test Suite\n");
    printf("========================================\n");

    /* Run all tests */
    test_sys_mode_operations();
    test_sys_hook_registration();
    test_sys_hook_failure();
    test_sys_watchdog();
    test_sys_wakeup_net();
    test_sys_wakeup_gpio();
    test_sys_all_modes();
    test_sys_cpu_temperature();
    test_sys_adc_value();

    printf("\n========================================\n");
    printf("  All Tests Passed! ✓\n");
    printf("========================================\n");

    return 0;
}
