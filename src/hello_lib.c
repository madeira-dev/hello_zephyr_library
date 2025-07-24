#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/*
in case I need these paths:
                "/Users/madeira/my_zephyr_projects/zephyr/include",
                "/Users/madeira/my_zephyr_projects/zephyr/include/zephyr",
                "/Users/madeira/my_zephyr_projects/zephyr/lib/libc/minimal/include",
                "/Users/madeira/zephyr-sdk-0.17.0/arm-zephyr-eabi/arm-zephyr-eabi/include"
*/

// Define a logging module for cleaner output
LOG_MODULE_REGISTER(hello_lib, LOG_LEVEL_DBG);

#include "hello_lib.h"

void hello_lib_say_hello(void)
{
    // Using Zephyr's logging API is better than printk
    LOG_INF("Hello World from my awesome Zephyr library!");
}