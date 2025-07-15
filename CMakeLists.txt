cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nrf-lteTest)

# TODO: Remove this when the modem firmware is included in the Zephyr SDK
# This is a workaround to ensure the modem firmware is available for the application.
# Force inclusion of modem firmware binary
# set(NRF_MODEM_FW_FILE ${CMAKE_CURRENT_SOURCE_DIR}/nrf_modem.bin)
# message(STATUS "Using modem firmware from: ${NRF_MODEM_FW_FILE}")

target_sources(app PRIVATE src/main.c)
