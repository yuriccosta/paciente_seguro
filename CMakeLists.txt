# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)


set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(paciente_seguro C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(paciente_seguro paciente_seguro.c lib/perifericos.c lib/ssd1306.c)

pico_set_program_name(paciente_seguro "paciente_seguro")
pico_set_program_version(paciente_seguro "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(paciente_seguro 0)
pico_enable_stdio_usb(paciente_seguro 1)

# Add the standard library to the build
target_link_libraries(paciente_seguro
    pico_stdlib
    hardware_adc
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
    pico_mbedtls
    pico_lwip_mbedtls
    hardware_pio
    hardware_pwm
    hardware_clocks
    hardware_i2c
    )

# Add the standard include files to the build
target_include_directories(paciente_seguro PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/lib
)

# Add any user requested libraries
target_link_libraries(paciente_seguro

)

pico_add_extra_outputs(paciente_seguro)

