/********************************** (C) COPYRIGHT *******************************
 * File Name          : system_ch32v30x.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/03/06
 * Description        : CH32V30x Device Peripheral Access Layer System Source File.
 *                      For HSE = 8Mhz
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#include "ch32v30x.h"

/*
 * Uncomment the line corresponding to the desired System clock (SYSCLK) frequency (after
 * reset the HSI is used as SYSCLK source).
 * If none of the define below is enabled, the HSI is used as System clock source.
 */
// #define SYSCLK_FREQ_HSE    HSE_VALUE
// #define SYSCLK_FREQ_48MHz_HSE  48000000
// #define SYSCLK_FREQ_56MHz_HSE  56000000
// #define SYSCLK_FREQ_72MHz_HSE  72000000
// #define SYSCLK_FREQ_96MHz_HSE  96000000
// #define SYSCLK_FREQ_120MHz_HSE  120000000
// #define SYSCLK_FREQ_144MHz_HSE 144000000
/* HSE = 16MHz, SYSCLK = 16M /5 *12 /5 *16 = 122.88MHz (PLL2 cascade)
 * USBHS PLL reference = HSE/4 = 4MHz (exact). */
#define SYSCLK_FREQ_122_88MHz_HSE 122880000
// #define SYSCLK_FREQ_HSI    HSI_VALUE
// #define SYSCLK_FREQ_48MHz_HSI  48000000
// #define SYSCLK_FREQ_56MHz_HSI  56000000
// #define SYSCLK_FREQ_72MHz_HSI  72000000
// #define SYSCLK_FREQ_96MHz_HSI  96000000
// #define SYSCLK_FREQ_120MHz_HSI  120000000
 //#define SYSCLK_FREQ_144MHz_HSI  144000000

/* Clock Definitions */
#ifdef SYSCLK_FREQ_HSE
uint32_t SystemCoreClock = SYSCLK_FREQ_HSE; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_48MHz_HSE
uint32_t SystemCoreClock = SYSCLK_FREQ_48MHz_HSE; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_56MHz_HSE
uint32_t SystemCoreClock = SYSCLK_FREQ_56MHz_HSE; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_72MHz_HSE
uint32_t SystemCoreClock = SYSCLK_FREQ_72MHz_HSE; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_96MHz_HSE
uint32_t SystemCoreClock = SYSCLK_FREQ_96MHz_HSE; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_120MHz_HSE
uint32_t SystemCoreClock = SYSCLK_FREQ_120MHz_HSE; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_144MHz_HSE
uint32_t SystemCoreClock = SYSCLK_FREQ_144MHz_HSE; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_122_88MHz_HSE
uint32_t SystemCoreClock = SYSCLK_FREQ_122_88MHz_HSE; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_48MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_48MHz_HSI; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_56MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_56MHz_HSI; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_72MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_72MHz_HSI; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_96MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_96MHz_HSI; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_120MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_120MHz_HSI; /* System Clock Frequency (Core Clock) */
#elif defined SYSCLK_FREQ_144MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_144MHz_HSI; /* System Clock Frequency (Core Clock) */
#else
uint32_t SystemCoreClock = HSI_VALUE; /* System Clock Frequency (Core Clock) */

#endif

__I uint8_t AHBPrescTable[16] =
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

/* system_private_function_proto_types */
static void SetSysClock(void);

#ifdef SYSCLK_FREQ_HSE
static void SetSysClockToHSE(void);
#elif defined SYSCLK_FREQ_48MHz_HSE
static void SetSysClockTo48_HSE(void);
#elif defined SYSCLK_FREQ_56MHz_HSE
static void SetSysClockTo56_HSE(void);
#elif defined SYSCLK_FREQ_72MHz_HSE
static void SetSysClockTo72_HSE(void);
#elif defined SYSCLK_FREQ_96MHz_HSE
static void SetSysClockTo96_HSE(void);
#elif defined SYSCLK_FREQ_120MHz_HSE
static void SetSysClockTo120_HSE(void);
#elif defined SYSCLK_FREQ_144MHz_HSE
static void SetSysClockTo144_HSE(void);
#elif defined SYSCLK_FREQ_122_88MHz_HSE
static void SetSysClockTo122_88_HSE(void);
#elif defined SYSCLK_FREQ_48MHz_HSI
static void SetSysClockTo48_HSI(void);
#elif defined SYSCLK_FREQ_56MHz_HSI
static void SetSysClockTo56_HSI(void);
#elif defined SYSCLK_FREQ_72MHz_HSI
static void SetSysClockTo72_HSI(void);
#elif defined SYSCLK_FREQ_96MHz_HSI
static void SetSysClockTo96_HSI(void);
#elif defined SYSCLK_FREQ_120MHz_HSI
static void SetSysClockTo120_HSI(void);
#elif defined SYSCLK_FREQ_144MHz_HSI
static uint8_t SetSysClockTo144_HSI(void);//函数类型修改

#endif


void SystemInit(void)
{
    RCC->CTLR |= (uint32_t)0x00000001;

    RCC->CFGR0 &= (uint32_t)0xF0FF0000;

    RCC->CTLR &= (uint32_t)0xFEF6FFFF;
    RCC->CTLR &= (uint32_t)0xFFFBFFFF;
    RCC->CFGR0 &= (uint32_t)0xFF00FFFF;

#ifdef CH32V30x_D8C
    RCC->CTLR &= (uint32_t)0xEBFFFFFF;
    RCC->INTR = 0x00FF0000;
    RCC->CFGR2 = 0x00000000;
#else
    RCC->INTR = 0x009F0000;
#endif
    SetSysClock();
}

/*********************************************************************
 * @fn      SystemCoreClockUpdate
 *
 * @brief   Update SystemCoreClock variable according to Clock Register Values.
 *
 * @return  none
 */
void SystemCoreClockUpdate(void)
{
    uint32_t tmp = 0, pllmull = 0, pllsource = 0;
    uint8_t Pll_6_5 = 0;

#ifdef CH32V30x_D8C
    uint8_t Pll2mull = 0;

#endif

    tmp = RCC->CFGR0 & RCC_SWS;

    switch (tmp)
    {
    case 0x00:
        SystemCoreClock = HSI_VALUE;
        break;
    case 0x04:
        SystemCoreClock = HSE_VALUE;
        break;
    case 0x08:
        pllmull = RCC->CFGR0 & RCC_PLLMULL;
        pllsource = RCC->CFGR0 & RCC_PLLSRC;
        pllmull = (pllmull >> 18) + 2;

#ifdef CH32V30x_D8
        if (pllmull == 17)
            pllmull = 18;
#else
        if (pllmull == 2)
            pllmull = 18;
        if (pllmull == 15)
        {
            pllmull = 13; /* *6.5 */
            Pll_6_5 = 1;
        }
        if (pllmull == 16)
            pllmull = 15;
        if (pllmull == 17)
            pllmull = 16;
#endif

        if (pllsource == 0x00)
        {
            if (EXTEN->EXTEN_CTR & EXTEN_PLL_HSI_PRE)
                SystemCoreClock = HSI_VALUE * pllmull;
            else
                SystemCoreClock = (HSI_VALUE >> 1) * pllmull;
        }
        else
        {

#ifdef CH32V30x_D8
            if ((RCC->CFGR0 & RCC_PLLXTPRE) != (uint32_t)RESET)
            {
                SystemCoreClock = (HSE_VALUE >> 1) * pllmull;
            }
            else
            {
                SystemCoreClock = HSE_VALUE * pllmull;
            }

#else
            if (RCC->CFGR2 & (1 << 16))
            {                                                                   /* PLL2 */
                SystemCoreClock = HSE_VALUE / (((RCC->CFGR2 & 0xF0) >> 4) + 1); /* PREDIV2 */

                Pll2mull = (uint8_t)((RCC->CFGR2 & 0xF00) >> 8);

                if (Pll2mull == 0)
                    SystemCoreClock = (SystemCoreClock * 5) >> 1;
                else if (Pll2mull == 1)
                    SystemCoreClock = (SystemCoreClock * 25) >> 1;
                else if (Pll2mull == 15)
                    SystemCoreClock = SystemCoreClock * 20;
                else
                    SystemCoreClock = SystemCoreClock * (Pll2mull + 2);

                SystemCoreClock = SystemCoreClock / ((RCC->CFGR2 & 0xF) + 1); /* PREDIV1 */
            }
            else
            {                                                           /* HSE */
                SystemCoreClock = HSE_VALUE / ((RCC->CFGR2 & 0xF) + 1); /* PREDIV1 */
            }

            SystemCoreClock = SystemCoreClock * pllmull;
#endif
        }

        if (Pll_6_5 == 1)
            SystemCoreClock = (SystemCoreClock / 2);

        break;
    default:
        SystemCoreClock = HSI_VALUE;
        break;
    }

    tmp = AHBPrescTable[((RCC->CFGR0 & RCC_HPRE) >> 4)];
    SystemCoreClock >>= tmp;
}

/*********************************************************************
 * @fn      SetSysClock
 *
 * @brief   Configures the System clock frequency, HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClock(void)
{
    // GPIO_IPD_Unused();
#ifdef SYSCLK_FREQ_HSE
    SetSysClockToHSE();
#elif defined SYSCLK_FREQ_48MHz_HSE
    SetSysClockTo48_HSE();
#elif defined SYSCLK_FREQ_56MHz_HSE
    SetSysClockTo56_HSE();
#elif defined SYSCLK_FREQ_72MHz_HSE
    SetSysClockTo72_HSE();
#elif defined SYSCLK_FREQ_96MHz_HSE
    SetSysClockTo96_HSE();
#elif defined SYSCLK_FREQ_120MHz_HSE
    SetSysClockTo120_HSE();
#elif defined SYSCLK_FREQ_144MHz_HSE
    SetSysClockTo144_HSE();
#elif defined SYSCLK_FREQ_122_88MHz_HSE
    SetSysClockTo122_88_HSE();
#elif defined SYSCLK_FREQ_48MHz_HSI
    SetSysClockTo48_HSI();
#elif defined SYSCLK_FREQ_56MHz_HSI
    SetSysClockTo56_HSI();
#elif defined SYSCLK_FREQ_72MHz_HSI
    SetSysClockTo72_HSI();
#elif defined SYSCLK_FREQ_96MHz_HSI
    SetSysClockTo96_HSI();
#elif defined SYSCLK_FREQ_120MHz_HSI
    SetSysClockTo120_HSI();
#elif defined SYSCLK_FREQ_144MHz_HSI
    SetSysClockTo144_HSI();

#endif

    /* If none of the define above is enabled, the HSI is used as System clock
     * source (default after reset)
     */
}

#ifdef SYSCLK_FREQ_HSE

/*********************************************************************
 * @fn      SetSysClockToHSE
 *
 * @brief   Sets HSE as System clock source and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockToHSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV1;

        /* Select HSE as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_HSE;

        /* Wait till HSE is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x04)
        {
        }
    }
    else
    {
        /* If HSE fails to start-up, the application will have wrong clock
         * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_48MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo48_HSE
 *
 * @brief   Sets System clock frequency to 48MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo48_HSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);
    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

        /*  PLL configuration: PLLCLK = HSE * 6 = 48 MHz */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL));

#ifdef CH32V30x_D8
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL6);
#else
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL6_EXTEN);
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while ((RCC->CTLR & RCC_PLLRDY) == 0)
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
         * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_56MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo56_HSE
 *
 * @brief   Sets System clock frequency to 56MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo56_HSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

        /* PLL configuration: PLLCLK = HSE * 7 = 56 MHz */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL));

#ifdef CH32V30x_D8
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL7);
#else
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL7_EXTEN);
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while ((RCC->CTLR & RCC_PLLRDY) == 0)
        {
        }

        /* Select PLL as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
         * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_72MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo72_HSE
 *
 * @brief   Sets System clock frequency to 72MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo72_HSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

        /* PLL configuration: PLLCLK = HSE * 9 = 72 MHz */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE |
                                             RCC_PLLMULL));

#ifdef CH32V30x_D8
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL9);
#else
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL9_EXTEN);
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while ((RCC->CTLR & RCC_PLLRDY) == 0)
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
         * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_96MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo96_HSE
 *
 * @brief   Sets System clock frequency to 96MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo96_HSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

        /*  PLL configuration: PLLCLK = HSE * 12 = 96 MHz */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE |
                                             RCC_PLLMULL));

#ifdef CH32V30x_D8
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL12);
#else
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL12_EXTEN);
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while ((RCC->CTLR & RCC_PLLRDY) == 0)
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
         * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_120MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo120_HSE
 *
 * @brief   Sets System clock frequency to 120MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo120_HSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

        /*  PLL configuration: PLLCLK = HSE * 15 = 120 MHz */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE |
                                             RCC_PLLMULL));

#ifdef CH32V30x_D8
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL15);
#else
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL15_EXTEN);
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while ((RCC->CTLR & RCC_PLLRDY) == 0)
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
         * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_144MHz_HSE

/*********************************************************************
 * @fn      SetSysClockTo144_HSE
 *
 * @brief   Sets System clock frequency to 144MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo144_HSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

        /*  PLL configuration: PLLCLK = HSE * 18 = 144 MHz */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE |
                                             RCC_PLLMULL));

#ifdef CH32V30x_D8
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL18);
#else
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL18_EXTEN);
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while ((RCC->CTLR & RCC_PLLRDY) == 0)
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
        {
        }

        //        /*  PLL3 configuration: PLLCLK = HSE * 7.5 = 96 MHz */
        //        RCC->CFGR2 &= (uint32_t) ((uint32_t) ~(RCC_PREDIV2 | RCC_PLL3MUL));
        //        RCC->CFGR2 |= (uint32_t) (RCC_PPRE2_DIV1 | RCC_PLL3Mul_12);
        //
        //        /* Enable PLL */
        //        RCC->CTLR |= RCC_PLL3ON;
        //        /* Wait till PLL is ready */
        //        while((RCC->CTLR & RCC_PLL3RDY) == 0)
        //        {
        //        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
         * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_122_88MHz_HSE


static void SetSysClockTo122_88_HSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK / 2 */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

        /* PREDIV2 = /5 (field 4), PLL2MUL = x12 (field 10 = 0xA00):
         * 16MHz / 5 * 12 = 38.4MHz */
        RCC->CFGR2 = (RCC->CFGR2 & ~(RCC_PREDIV2 | RCC_PLL2MUL)) |
                     (uint32_t)0x00000040 | (uint32_t)0x00000A00;
        /* PREDIV1 source = PLL2 (bit16), PREDIV1 = /5 (field 4):
         * 38.4MHz / 5 = 7.68MHz feeds the main PLL */
        RCC->CFGR2 = (RCC->CFGR2 & ~(RCC_PREDIV1 | RCC_PREDIV1SRC)) |
                     (uint32_t)0x00000004 | RCC_PREDIV1SRC;

        /* Enable PLL2 and wait for lock */
        RCC->CTLR |= RCC_PLL2ON;
        while ((RCC->CTLR & RCC_PLL2RDY) == 0)
        {
        }

        /* Main PLL: input = PREDIV1 output, PLLCLK = 7.68MHz * 16 = 122.88MHz */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE |
                                             RCC_PLLMULL));
#ifdef CH32V30x_D8
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL16);
#else
        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL16_EXTEN);
#endif

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while ((RCC->CTLR & RCC_PLLRDY) == 0)
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
         * configuration. User can add here some code to deal with this error
         */
    }
}

#elif defined SYSCLK_FREQ_48MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo48_HSI
 *
 * @brief   Sets System clock frequency to 48MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo48_HSI(void)
{
    EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
    /* PCLK2 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
    /* PCLK1 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

    /*  PLL configuration: PLLCLK = HSI * 6 = 48 MHz */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL));

#ifdef CH32V30x_D8
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL6);
#else
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL6_EXTEN);
#endif

    /* Enable PLL */
    RCC->CTLR |= RCC_PLLON;
    /* Wait till PLL is ready */
    while ((RCC->CTLR & RCC_PLLRDY) == 0)
    {
    }
    /* Select PLL as system clock source */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
    {
    }
}

#elif defined SYSCLK_FREQ_56MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo56_HSI
 *
 * @brief   Sets System clock frequency to 56MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo56_HSI(void)
{
    EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
    /* PCLK2 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
    /* PCLK1 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

    /*  PLL configuration: PLLCLK = HSI * 7 = 56 MHz */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL));

#ifdef CH32V30x_D8
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL7);
#else
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL7_EXTEN);
#endif

    /* Enable PLL */
    RCC->CTLR |= RCC_PLLON;
    /* Wait till PLL is ready */
    while ((RCC->CTLR & RCC_PLLRDY) == 0)
    {
    }
    /* Select PLL as system clock source */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
    {
    }
}

#elif defined SYSCLK_FREQ_72MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo72_HSI
 *
 * @brief   Sets System clock frequency to 72MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo72_HSI(void)
{
    EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
    /* PCLK2 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
    /* PCLK1 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

    /*  PLL configuration: PLLCLK = HSI * 9 = 72 MHz */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL));

#ifdef CH32V30x_D8
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL9);
#else
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL9_EXTEN);
#endif

    /* Enable PLL */
    RCC->CTLR |= RCC_PLLON;
    /* Wait till PLL is ready */
    while ((RCC->CTLR & RCC_PLLRDY) == 0)
    {
    }
    /* Select PLL as system clock source */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
    {
    }
}

#elif defined SYSCLK_FREQ_96MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo96_HSI
 *
 * @brief   Sets System clock frequency to 96MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo96_HSI(void)
{
    EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
    /* PCLK2 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
    /* PCLK1 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

    /*  PLL configuration: PLLCLK = HSI * 12 = 96 MHz */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL));

#ifdef CH32V30x_D8
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL12);
#else
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL12_EXTEN);
#endif

    /* Enable PLL */
    RCC->CTLR |= RCC_PLLON;
    /* Wait till PLL is ready */
    while ((RCC->CTLR & RCC_PLLRDY) == 0)
    {
    }
    /* Select PLL as system clock source */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
    {
    }
}

#elif defined SYSCLK_FREQ_120MHz_HSI

/*********************************************************************
 * @fn      SetSysClockTo120_HSI
 *
 * @brief   Sets System clock frequency to 120MHz and configure HCLK, PCLK2 and PCLK1 prescalers.
 *
 * @return  none
 */
static void SetSysClockTo120_HSI(void)
{
    EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
    /* PCLK2 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
    /* PCLK1 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

    /*  PLL configuration: PLLCLK = HSI * 15 = 120 MHz */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL));

#ifdef CH32V30x_D8
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL15);
#else
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL15_EXTEN);
#endif

    /* Enable PLL */
    RCC->CTLR |= RCC_PLLON;
    /* Wait till PLL is ready */
    while ((RCC->CTLR & RCC_PLLRDY) == 0)
    {
    }
    /* Select PLL as system clock source */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
    {
    }
}

#elif defined SYSCLK_FREQ_144MHz_HSI


static uint8_t SetSysClockTo144_HSI(void)
{
    /* ==================== 1. 先配置 PLL3 (原 My_PLL3_Config 逻辑) ==================== */
    __IO uint32_t StartUpCounter = 0;
    uint32_t HSEStatus = 0;
    
    /* 使能 HSE 外部高速晶振 */
    RCC->CTLR |= (uint32_t)(1 << 16); // HSEON 位置 1 (第16位)

    /* 等待 HSE 就绪或超时退出 */
    do
    {
        HSEStatus = RCC->CTLR & (1 << 17); // 读取 HSERDY 位 (第17位)
        StartUpCounter++;
    } while ((HSEStatus == 0) && (StartUpCounter != 0x5000));

    // 如果 HSE 启动失败，直接退出并返回 0，不再配置系统时钟
    if ((RCC->CTLR & (1 << 17)) == 0)
    {
        return 0; 
    }

    /* 配置 PREDIV2 和 PLL3MUL 之前，必须先关闭 PLL3 */
    RCC->CTLR &= ~(uint32_t)(1 << 28); // PLL3ON 清 0 (第28位)

    /* 清除并配置 RCC_CFGR2 寄存器中的 PREDIV2 和 PLL3MUL 位 */
    RCC->CFGR2 &= ~(uint32_t)((0xF << 12) | (0xF << 4));
    
    // PREDIV2 配置为 2分频: 0x1 (移位到位 [7:4])
    // PLL3MUL  配置为 8倍频: 0x6 (移位到位 [15:12])
    RCC->CFGR2 |= (uint32_t)((0x6 << 12) | (0x1 << 4));

    /* 使能 PLL3 */
    RCC->CTLR |= (uint32_t)(1 << 28); // PLL3ON 位置 1 (第28位)

    /* 轮询 RCC_CTLR 的 PLL3RDY 位（第29位），直到 PLL3 锁定 */
    while ((RCC->CTLR & (1 << 29)) == 0)
    {
        // 硬件未将 PLL3RDY 位置 1 时，在此死循环死等
    }


    /* ==================== 2. 再配置系统时钟 (原系统时钟配置逻辑) ==================== */
    EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;

    /* HCLK = SYSCLK */
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
    /* PCLK2 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
    /* PCLK1 = HCLK */
    RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

    /*  PLL configuration: PLLCLK = HSI * 18 = 144 MHz */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL));

#ifdef CH32V30x_D8
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL18);
#else
    RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL18_EXTEN);
#endif

    /* Enable PLL */
    RCC->CTLR |= RCC_PLLON;
    /* Wait till PLL is ready */
    while ((RCC->CTLR & RCC_PLLRDY) == 0)
    {
        }
    /* Select PLL as system clock source */
    RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
    {
    }

    return 1; // 全部配置成功
}
#endif
