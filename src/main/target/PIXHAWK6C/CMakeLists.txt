# Option to disable IOMCU for initial testing
option(DISABLE_IOMCU "Disable IOMCU coprocessor support" OFF)

# Option to build firmware compatible with ArduPilot/PX4 bootloader
option(USE_ARDUPILOT_BOOTLOADER "Build firmware compatible with ArduPilot/PX4 128KB bootloader" OFF)

# Select linker script based on bootloader option
if(USE_ARDUPILOT_BOOTLOADER)
    set(PIXHAWK6C_LINKER_SCRIPT stm32_flash_h743xi_for_bl)
    message(STATUS "PIXHAWK6C: Building for ArduPilot/PX4 bootloader (firmware starts at 0x08020000)")
else()
    set(PIXHAWK6C_LINKER_SCRIPT stm32_flash_h743xi)
    message(STATUS "PIXHAWK6C: Building standalone firmware (starts at 0x08000000)")
endif()

target_stm32h743xi(PIXHAWK6C HSE_MHZ 16 LINKER_SCRIPT ${PIXHAWK6C_LINKER_SCRIPT})
target_stm32h743xi(PIXHAWK6C_MINI HSE_MHZ 16 LINKER_SCRIPT ${PIXHAWK6C_LINKER_SCRIPT})

if(NOT DISABLE_IOMCU)
    # Include IOMCU firmware build support
    include(iomcu-firmware)

    # Enable IOMCU for PIXHAWK6C targets
    set(PIXHAWK6C_USE_IOMCU ON)
    set(PIXHAWK6C_MINI_USE_IOMCU ON)

    # Define USE_IOMCU for the compiler
    target_compile_definitions(PIXHAWK6C.elf PRIVATE USE_IOMCU)
    target_compile_definitions(PIXHAWK6C_MINI.elf PRIVATE USE_IOMCU)

    # Add IOMCU driver sources to both targets
    target_sources(PIXHAWK6C.elf PRIVATE
        ${CMAKE_SOURCE_DIR}/src/main/drivers/iomcu/iomcu.c
        ${CMAKE_SOURCE_DIR}/src/main/drivers/iomcu/iomcu_uploader.c
        ${CMAKE_SOURCE_DIR}/src/main/drivers/pwm_output_iomcu.c
    )
    target_sources(PIXHAWK6C_MINI.elf PRIVATE
        ${CMAKE_SOURCE_DIR}/src/main/drivers/iomcu/iomcu.c
        ${CMAKE_SOURCE_DIR}/src/main/drivers/iomcu/iomcu_uploader.c
        ${CMAKE_SOURCE_DIR}/src/main/drivers/pwm_output_iomcu.c
    )

    # Build and embed IOMCU firmware for both variants
    build_iomcu_firmware(PIXHAWK6C)
    build_iomcu_firmware(PIXHAWK6C_MINI)

    message(STATUS "PIXHAWK6C: IOMCU support enabled")
else()
    message(STATUS "PIXHAWK6C: IOMCU support disabled")
endif()
