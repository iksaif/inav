/*
 * STM32F103 Startup Code for IOMCU Firmware
 */

.syntax unified
.cpu cortex-m3
.thumb

/* Vector Table */
.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
.size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack                    /* Top of Stack */
    .word Reset_Handler              /* Reset Handler */
    .word NMI_Handler                /* NMI Handler */
    .word HardFault_Handler          /* Hard Fault Handler */
    .word MemManage_Handler          /* MPU Fault Handler */
    .word BusFault_Handler           /* Bus Fault Handler */
    .word UsageFault_Handler         /* Usage Fault Handler */
    .word 0                          /* Reserved */
    .word 0                          /* Reserved */
    .word 0                          /* Reserved */
    .word 0                          /* Reserved */
    .word SVC_Handler                /* SVCall Handler */
    .word DebugMon_Handler           /* Debug Monitor Handler */
    .word 0                          /* Reserved */
    .word PendSV_Handler             /* PendSV Handler */
    .word SysTick_Handler            /* SysTick Handler */

    /* External Interrupts */
    .word 0                          /* WWDG */
    .word 0                          /* PVD */
    .word 0                          /* TAMPER */
    .word 0                          /* RTC */
    .word 0                          /* FLASH */
    .word 0                          /* RCC */
    .word 0                          /* EXTI0 */
    .word 0                          /* EXTI1 */
    .word 0                          /* EXTI2 */
    .word 0                          /* EXTI3 */
    .word 0                          /* EXTI4 */
    .word 0                          /* DMA1_Channel1 */
    .word 0                          /* DMA1_Channel2 */
    .word 0                          /* DMA1_Channel3 */
    .word 0                          /* DMA1_Channel4 */
    .word 0                          /* DMA1_Channel5 */
    .word 0                          /* DMA1_Channel6 */
    .word 0                          /* DMA1_Channel7 */
    .word 0                          /* ADC1_2 */
    .word 0                          /* USB_HP_CAN1_TX */
    .word 0                          /* USB_LP_CAN1_RX0 */
    .word 0                          /* CAN1_RX1 */
    .word 0                          /* CAN1_SCE */
    .word 0                          /* EXTI9_5 */
    .word 0                          /* TIM1_BRK */
    .word 0                          /* TIM1_UP */
    .word 0                          /* TIM1_TRG_COM */
    .word 0                          /* TIM1_CC */
    .word 0                          /* TIM2 */
    .word 0                          /* TIM3 */
    .word 0                          /* TIM4 */
    .word 0                          /* I2C1_EV */
    .word 0                          /* I2C1_ER */
    .word 0                          /* I2C2_EV */
    .word 0                          /* I2C2_ER */
    .word 0                          /* SPI1 */
    .word 0                          /* SPI2 */
    .word 0                          /* USART1 */
    .word 0                          /* USART2 */
    .word 0                          /* USART3 */
    .word 0                          /* EXTI15_10 */
    .word 0                          /* RTCAlarm */
    .word 0                          /* USBWakeUp */

/* Reset Handler */
.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    /* Copy data from Flash to RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b copy_data_loop

copy_data_init:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

copy_data_loop:
    adds r4, r0, r3
    cmp r4, r1
    bcc copy_data_init

    /* Zero BSS */
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b zero_bss_loop

zero_bss_init:
    str r3, [r2]
    adds r2, r2, #4

zero_bss_loop:
    cmp r2, r4
    bcc zero_bss_init

    /* Call main */
    bl main
    bx lr

.size Reset_Handler, .-Reset_Handler

/* Default Handlers */
.section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
    b Infinite_Loop
.size Default_Handler, .-Default_Handler

/* Weak aliases for handlers */
.weak NMI_Handler
.thumb_set NMI_Handler,Default_Handler

.weak HardFault_Handler
.thumb_set HardFault_Handler,Default_Handler

.weak MemManage_Handler
.thumb_set MemManage_Handler,Default_Handler

.weak BusFault_Handler
.thumb_set BusFault_Handler,Default_Handler

.weak UsageFault_Handler
.thumb_set UsageFault_Handler,Default_Handler

.weak SVC_Handler
.thumb_set SVC_Handler,Default_Handler

.weak DebugMon_Handler
.thumb_set DebugMon_Handler,Default_Handler

.weak PendSV_Handler
.thumb_set PendSV_Handler,Default_Handler

.weak SysTick_Handler
.thumb_set SysTick_Handler,Default_Handler
