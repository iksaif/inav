# Include IOMCU firmware build support
include(iomcu-firmware)

# Enable IOMCU for PIXHAWK6C targets
set(PIXHAWK6C_USE_IOMCU ON)
set(PIXHAWK6C_MINI_USE_IOMCU ON)

target_stm32h743xi(PIXHAWK6C HSE_MHZ 16)
target_stm32h743xi(PIXHAWK6C_MINI HSE_MHZ 16)

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
