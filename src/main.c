#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>
#include <modem/modem_key_mgmt.h>

#include <net/aws_iot.h>
#include <date_time.h>
#include <nrf_socket.h>
#include <stdio.h>
#include <inttypes.h>

#include <hal/nrf_gpio.h>

#define MODEM_POWER_PIN 12

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF); 

static void lte_handler(const struct lte_lc_evt *const evt)
{
        LOG_INF("LTE Event: Type %d", evt->type);
}

int main(void)
{
        int err = 0;

        LOG_INF("Delaying startup for 5 seconds to allow the modem to initialize");
        k_sleep(K_SECONDS(5));

        LOG_INF("Modem power on, initializing...");
        err = nrf_modem_lib_init();
        if(err < 0) {
                LOG_ERR("Failed to initialize modem library: %d", err);
                return 1;
        }

        char fw_version[32] = {0};
        err = modem_info_string_get(MODEM_INFO_FW_VERSION, fw_version, sizeof(fw_version));
        if (err < 0) {
                LOG_ERR("Failed to get modem firmware version: %d", err);
        } else {
                LOG_INF("Modem firmware version: %s", fw_version);
        }


        LOG_INF("Calling lte_lc_connect_async");
        err = lte_lc_connect_async(lte_handler);
        if (err){
                LOG_ERR("Failed to connect to LTE network: %d", err);
                return 1;
        }

        while (true) {
                k_sleep(K_SECONDS(1));
                LOG_INF("BEEP!");
        }
}