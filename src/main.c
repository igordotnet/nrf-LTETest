/* Included Libraries */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>
#include <modem/modem_key_mgmt.h>
// #include <modem/at_cmd.h>

#include <nrf_socket.h>
#include <stdio.h>
#include <inttypes.h>

#include <hal/nrf_gpio.h>

#define MODEM_POWER_PIN  12

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static struct k_sem lte_connected = Z_SEM_INITIALIZER(lte_connected, 0, 1);
static struct k_work reconnect_work;

static bool connection_stable = false;

/* Function Declarations */
void modem_power_on(void);
void configure_lte_mode(void);
static void lte_handler(const struct lte_lc_evt *const evt);

/* Modem Power Control */
void modem_power_on(void) 
{
    LOG_INF("Powering on modem...");
    nrf_gpio_cfg_output(MODEM_POWER_PIN);
    nrf_gpio_pin_set(MODEM_POWER_PIN);
    k_sleep(K_SECONDS(2));
    LOG_INF("Modem power stabilized");
}

/* LTE Mode Configuration */
void configure_lte_mode(void)
{
    int err;

    LOG_INF("Configuring LTE mode...");

    // Try LTE-M first
    err = lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_LTEM, LTE_LC_SYSTEM_MODE_PREFER_AUTO);
    if (err) {
        LOG_WRN("Failed to set LTE-M mode: %d", err);
    }

    k_sleep(K_SECONDS(1));
}

/* Enhanced LTE Event Handler */
static void lte_handler(const struct lte_lc_evt *const evt)
{
    LOG_INF("LTE Event: Type %d", evt->type);

    switch (evt->type) {
    case LTE_LC_EVT_NW_REG_STATUS:
        LOG_INF("LTE Event: NW_REG_STATUS - Status: %d", evt->nw_reg_status);

        switch(evt->nw_reg_status) {
        case LTE_LC_NW_REG_REGISTERED_HOME:
        case LTE_LC_NW_REG_REGISTERED_ROAMING:
            LOG_INF("LTE connected successfully");
            connection_stable = true;
            k_sem_give(&lte_connected);
            break;

        case LTE_LC_NW_REG_NOT_REGISTERED:
            LOG_WRN("Not registered - will attempt reconnection");
            if (connection_stable) {
                k_work_submit(&reconnect_work);
            }
            break;

        case LTE_LC_NW_REG_REGISTRATION_DENIED:
            LOG_ERR("Registration denied - check SIM/APN/Network");
            break;

        case LTE_LC_NW_REG_SEARCHING:
            LOG_INF("Searching for network...");
            break;

        default:
            LOG_INF("Registration status: %d", evt->nw_reg_status);
            break;
        }
        break;

    case LTE_LC_EVT_PSM_UPDATE:
        LOG_INF("LTE Event: PSM_UPDATE - TAU: %d, Active Time: %d",
                evt->psm_cfg.tau, evt->psm_cfg.active_time);
        break;

    case LTE_LC_EVT_EDRX_UPDATE:
        LOG_INF("LTE Event: EDRX_UPDATE - eDRX: %f, PTW: %f",
                (double)evt->edrx_cfg.edrx, (double)evt->edrx_cfg.ptw);
        break;

    case LTE_LC_EVT_RRC_UPDATE:
        LOG_INF("LTE Event: RRC_UPDATE - RRC Mode: %s",
                evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
        break;

    case LTE_LC_EVT_CELL_UPDATE:
        LOG_INF("LTE Event: CELL_UPDATE - Cell ID: %d, TAC: %d",
                evt->cell.id, evt->cell.tac);
        break;

    case LTE_LC_EVT_LTE_MODE_UPDATE:
        LOG_INF("LTE Event: LTE_MODE_UPDATE - LTE Mode: %d", evt->lte_mode);
        break;

    case LTE_LC_EVT_TAU_PRE_WARNING:
        LOG_INF("LTE Event: TAU_PRE_WARNING");
        break;

    default:
        LOG_WRN("LTE Event: Unknown event type: %d", evt->type);
        break;
    }
}

/* Main Function */
int main(void)
{
    int err = 0;
    char uuid[64] = {0};

    LOG_INF("=== Starting nRF9160 LTE Test Application ===");
    LOG_INF("nRF Connect SDK v3.0.2");


    LOG_INF("Delaying startup for modem power stabilization...");
    k_sleep(K_SECONDS(5));

    // Power on modem (if needed)
    // modem_power_on();

    LOG_INF("Initializing modem library...");
    err = nrf_modem_lib_init();
    if (err < 0) {
        LOG_ERR("Modem library initialization failed: %d", err);
        return 1;
    }

    // Additional delay for modem initialization
    k_sleep(K_SECONDS(3));

    // Initialize modem info
    LOG_INF("Initializing modem info...");
    err = modem_info_init();
    if (err) {
        LOG_ERR("Modem info initialization failed: %d", err);
        return 1;
    }

    // Wait longer before getting UUID
    k_sleep(K_SECONDS(2));

    err = modem_info_get_fw_uuid(uuid, sizeof(uuid));
    if (err) {
        LOG_ERR("Failed to get modem UUID: %d", err);
        return 1;
    }
    LOG_INF("Modem UUID: %s", uuid);

    // Configure LTE mode before checking modem status
    configure_lte_mode();

    // Wait after mode setting
    k_sleep(K_SECONDS(2));

    LOG_INF("Attempting LTE connection...");
    err = lte_lc_connect_async(lte_handler);
    if (err) {
        LOG_ERR("lte_lc_connect_async failed: %d", err);
        return 1;
    }

    LOG_INF("Waiting for LTE connection...");

    // Add timeout for connection attempt
    if (k_sem_take(&lte_connected, K_SECONDS(120)) != 0) {
        LOG_ERR("LTE connection timeout after 120 seconds");
        return 1;
    }

    LOG_INF("LTE connection established successfully!");

    LOG_INF("=== LTE Connection Test Complete ===");
    LOG_INF("Device is now connected to the cellular network");

    // Main loop - keep connection alive and monitor status
    int loop_count = 0;
    while (1) {
        k_sleep(K_SECONDS(30));
        loop_count++;

        LOG_INF("Status check #%d - Connection stable: %s", 
                loop_count, connection_stable ? "YES" : "NO");

        // Periodic signal quality check
        if (loop_count % 10 == 0) {  // Every 5 minutes
            int rsrp = 0;
            if (modem_info_short_get(MODEM_INFO_RSRP, &rsrp) == 0) {
                LOG_INF("Current RSRP: %d dBm", rsrp - 140);
            }
        }
    }

    return 0;
}