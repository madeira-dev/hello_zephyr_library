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
    int num = 1;
    int addition = 2;

    LOG_INF("number is: %d", num);
    LOG_INF("addition is: %d", addition);
    LOG_INF("num+addition is: %d", (num + addition));
}