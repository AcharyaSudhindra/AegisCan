# Top-level ESP-IDF CMake project file for Aegis-CPS Tier 2
# Target: ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)

cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(aegis_cps_esp)
