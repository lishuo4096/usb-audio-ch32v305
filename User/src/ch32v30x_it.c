/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v30x_it.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/03/06
 * Description        : Main Interrupt Service Routines.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#include "ch32v30x_it.h"
#include "volume_app.h"
#include <stdio.h>

void NMI_Handler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));
void HardFault_Handler (void) __attribute__ ((interrupt ("WCH-Interrupt-fast")));

static void FaultReport (const char *tag) {
    uint32_t mcause, mepc, mtval, mstatus, sp, intsy;

    __asm volatile ("csrr %0, mcause" : "=r"(mcause));
    __asm volatile ("csrr %0, mepc" : "=r"(mepc));
    __asm volatile ("csrr %0, mtval" : "=r"(mtval));
    __asm volatile ("csrr %0, mstatus" : "=r"(mstatus));
    __asm volatile ("csrr %0, 0x804" : "=r"(intsy));
    __asm volatile ("mv   %0, sp" : "=r"(sp));

    printf ("\r\n!!! %s\r\n", tag);
    printf ("!!! mcause=%08X mepc=%08X mtval=%08X\r\n",
            (unsigned)mcause, (unsigned)mepc, (unsigned)mtval);
    printf ("!!! mstatus=%08X sp=%08X intsyerscr=%08X\r\n",
            (unsigned)mstatus, (unsigned)sp, (unsigned)intsy);
}

void NMI_Handler (void) {
    FaultReport ("NMI");
    while (1) {
        IWDG_WriteAccessCmd (IWDG_WriteAccess_Enable);
        IWDG_ReloadCounter();
    }
}

void HardFault_Handler (void) {
    FaultReport ("HARDFAULT");
    while (1) {
        /* 喂狗保命：不让看门狗把现场冲掉，方便接调试器 / 反复读日志 */
        IWDG_WriteAccessCmd (IWDG_WriteAccess_Enable);
        IWDG_ReloadCounter();
    }
}
