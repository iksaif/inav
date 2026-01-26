# IOMCU Firmware Build Integration
# Builds STM32F103 IOMCU firmware and embeds it into FMU firmware

function(build_iomcu_firmware TARGET_NAME)
    # Only build if USE_IOMCU is enabled for this target
    if(NOT ${TARGET_NAME}_USE_IOMCU)
        return()
    endif()

    message(STATUS "Configuring IOMCU firmware for ${TARGET_NAME}")

    # IOMCU firmware target name
    set(IOMCU_TARGET "iomcu_firmware_${TARGET_NAME}")
    set(IOMCU_ELF "${CMAKE_BINARY_DIR}/${IOMCU_TARGET}.elf")
    set(IOMCU_BIN "${CMAKE_BINARY_DIR}/${IOMCU_TARGET}.bin")
    set(IOMCU_EMBEDDED_C "${CMAKE_BINARY_DIR}/${IOMCU_TARGET}_embedded.c")

    # IOMCU source files
    set(IOMCU_SOURCES
        ${CMAKE_SOURCE_DIR}/src/main/iomcu_firmware/iomcu_main.c
        ${CMAKE_SOURCE_DIR}/src/main/iomcu_firmware/iomcu_protocol_handler.c
        ${CMAKE_SOURCE_DIR}/src/main/iomcu_firmware/iomcu_registers.c
        ${CMAKE_SOURCE_DIR}/src/main/iomcu_firmware/iomcu_pwm.c
        ${CMAKE_SOURCE_DIR}/src/main/iomcu_firmware/iomcu_safety.c
    )

    # IOMCU compiler flags for STM32F103RBT6
    set(IOMCU_CFLAGS
        -mcpu=cortex-m3
        -mthumb
        -mfloat-abi=soft
        -DSTM32F103xB
        -DIOMCU_BUILD
        -Os
        -ffunction-sections
        -fdata-sections
        -Wall
        -Werror
        -std=c11
    )

    # IOMCU linker flags
    set(IOMCU_LDFLAGS
        -mcpu=cortex-m3
        -mthumb
        -specs=nano.specs
        -specs=nosys.specs
        -Wl,--gc-sections
        -Wl,-Map=${CMAKE_BINARY_DIR}/${IOMCU_TARGET}.map
        -T${CMAKE_SOURCE_DIR}/src/main/iomcu_firmware/stm32f1xx_iomcu.ld
    )

    # IOMCU include directories
    set(IOMCU_INCLUDES
        ${CMAKE_SOURCE_DIR}/src/main/iomcu_firmware
        ${CMAKE_SOURCE_DIR}/src/main/drivers/iomcu
        ${CMAKE_SOURCE_DIR}/src/main/common
        ${CMAKE_SOURCE_DIR}/src/main/drivers
    )

    # Set IOMCU firmware size limit (124KB = 126976 bytes)
    set(IOMCU_MAX_SIZE 126976)

    # Create IOMCU firmware executable
    add_executable(${IOMCU_TARGET}
        ${IOMCU_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/main/iomcu_firmware/startup_stm32f103.s
    )

    # Set IOMCU compile options
    target_compile_options(${IOMCU_TARGET} PRIVATE ${IOMCU_CFLAGS})
    target_include_directories(${IOMCU_TARGET} PRIVATE ${IOMCU_INCLUDES})
    target_link_options(${IOMCU_TARGET} PRIVATE ${IOMCU_LDFLAGS})

    # Force output to CMAKE_BINARY_DIR instead of subdirectory
    set_target_properties(${IOMCU_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        OUTPUT_NAME ${IOMCU_TARGET}
        SUFFIX ".elf"
    )

    # Generate binary from ELF and check size
    add_custom_command(
        OUTPUT ${IOMCU_BIN}
        COMMAND ${CMAKE_OBJCOPY} -O binary ${IOMCU_ELF} ${IOMCU_BIN}
        COMMAND bash -c "size=\$\(stat -f%z ${IOMCU_BIN} 2>/dev/null || stat -c%s ${IOMCU_BIN}\) && \
            echo \"IOMCU firmware size: \$size bytes (max ${IOMCU_MAX_SIZE})\" && \
            test \$size -le ${IOMCU_MAX_SIZE} || \
            \(echo \"ERROR: IOMCU firmware too large\" && exit 1\)"
        DEPENDS ${IOMCU_TARGET}
        COMMENT "Generating IOMCU binary: ${IOMCU_BIN}"
        VERBATIM
    )

    # Convert binary to C array
    add_custom_command(
        OUTPUT ${IOMCU_EMBEDDED_C}
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/src/utils/bin2c.py
            ${IOMCU_BIN}
            ${IOMCU_EMBEDDED_C}
            iomcu_firmware_bin
        DEPENDS ${IOMCU_BIN}
        COMMENT "Embedding IOMCU firmware as C array"
        VERBATIM
    )

    # Create custom target for embedded firmware
    add_custom_target(
        ${IOMCU_TARGET}_embedded
        DEPENDS ${IOMCU_EMBEDDED_C}
    )

    # Add embedded firmware to main target ELF
    target_sources(${TARGET_NAME}.elf PRIVATE ${IOMCU_EMBEDDED_C})
    add_dependencies(${TARGET_NAME}.elf ${IOMCU_TARGET}_embedded)

    # Display ELF size info after build
    add_custom_command(
        TARGET ${IOMCU_TARGET} POST_BUILD
        COMMAND ${CMAKE_SIZE} ${IOMCU_ELF}
        COMMENT "IOMCU firmware ELF size"
        VERBATIM
    )

    message(STATUS "IOMCU firmware configured for ${TARGET_NAME}")
endfunction()
