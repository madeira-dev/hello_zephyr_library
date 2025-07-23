#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// Define a logging module for cleaner output
LOG_MODULE_REGISTER(hello_lib, LOG_LEVEL_DBG);

#include "hello_lib.h"

void hello_lib_say_hello(void)
{
    // Using Zephyr's logging API is better than printk
    LOG_INF("Hello World from my awesome Zephyr library!");
}