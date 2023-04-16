/**
 ******************************************************************************
 * @file    LwIP/LwIP_HTTP_Server_Netconn_RTOS/Src/main.c
 * @author  MCD Application Team
 * @brief   This sample code implements a http server application based on
 *          Netconn API of LwIP stack and FreeRTOS. This application uses
 *          STM32H7xx the ETH HAL API to transmit and receive data.
 *          The communication is done with a web browser of a remote PC.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "ethernetif.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "app_ethernet.h"

#include "retarget.h"
#include "tasks/user_tasks.h"

#include "utils.h"

#include "stm32h7xx_hal.h"

#include "stm32h7xx_hal_tim.h"
#include "stm32h7xx_hal_tim_ex.h"

#include "cli.h"

#include <math.h>

#include "flexptp/ptp_defs.h"

#include "persistent_storage.h"

#include "flexptp/ptp_core.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define OSC_XTAL (1)
#define OSC_EXT (2)

#if HSE_VALUE == 16000000UL
#define OSC_TYPE (OSC_XTAL)
#else
#define OSC_TYPE (OSC_EXT)
#endif

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
struct netif gnetif; /* network interface structure */

// Devices handles
static UART_HandleTypeDef shUART3;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void BSP_Config(void);
static void StartThread(void const *argument);
static void Netif_Config(void);
static void MPU_Config(void);
static void CPU_CACHE_Enable(void);

/* Private functions ---------------------------------------------------------*/

static TaskHandle_t shBoardTask;
static void task_board(void const *argument);

static void reg_board_task() {
    BaseType_t result = xTaskCreate(task_board, "board", 64, NULL, 1, &shBoardTask);
    if (result != pdPASS) { // taszk létrehozása
        MSG("Failed to create task! (errcode: %ld)\n", result);
    }
}

static void BSP_Config(void) {
    BSP_LED_Init(LED2);
    BSP_LED_Init(LED3);
    BSP_LED_Init(LED1);
}

static bool sPing = false;
static int CB_ping(const CliToken_Type *ppArgs, uint8_t argc) {
    if (argc >= 1) {
        sPing = (ONOFF(ppArgs[0]) > 0);
    } else {
        sPing = !sPing;
    }

    return 0;
}

static char sTaskPrintBuf[512];

static int CB_listTasks(const CliToken_Type *ppArgs, uint8_t argc) {
    osThreadList(sTaskPrintBuf);
    printf("\nName\t\tState\tPrio\tStk\t#\n\n");
    printf("%s\n\n", sTaskPrintBuf);
    printf("B : Blocked, R : Ready, D : Deleted, S : Suspended\n\n");

    return 0;
}

static int CB_config(const CliToken_Type *ppArgs, uint8_t argc) {
    if (!strcmp(ppArgs[0], "save")) {
        MSG("Saving to persistent storage...");

        // clear storage
        ps_clear();

        // save PTP-config
        PtpConfig config;
        ptp_store_config(&config);
        ps_store(CONFIG_PTP, &config);

        MSG("done!\n");

        return 0;
    } else if (!strcmp(ppArgs[0], "load")) {
        MSG("Loading from persistent storage...");

        // load PTP-config
        ptp_load_config_from_dump(ps_load(CONFIG_PTP));

        MSG("done!\n");

        return 0;
    } else if (!strcmp(ppArgs[0], "clear")) {
        MSG("Clearing persistent storage...");
        ps_clear();
        MSG("done!\n");

        return 0;
    }

    return -1;
}

//static int CB_start_stop_ptp(const CliToken_Type *ppArgs, uint8_t argc) {
//    if (!strcmp(ppArgs[0], "start")) {
//        MSG("Starting PTP!\n");
//        reg_task_ptp();
//    } else if (!strcmp(ppArgs[0], "stop")) {
//        unreg_task_ptp();
//        MSG("PTP stopped!\n");
//    } else {
//        return -1;
//    }
//    return 0;
//}

static void task_board(void const *argument) {
    sPing = false;

    // register cli commands
    cli_register_command("ping [on|off] \t\t\tTurn on/off ping led blinking", 1, 0, CB_ping);
    cli_register_command("tasks \t\t\tPrint list or registered tasks", 1, 0, CB_listTasks);
    cli_register_command("config {save|load|clear} \t\t\tSave/load/clear config to/from persistent storage", 1, 1, CB_config);
//    cli_register_command("ptp {start|stop} \t\t\tStart PTP", 1, 1, CB_start_stop_ptp);

    // construct config table
    ps_add_entry(sizeof(PtpConfig), CONFIG_PTP);

    while (true) {
        if (sPing || BSP_LED_GetState(LED1)) {
            BSP_LED_Toggle(LED1);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        if (BSP_PB_GetState(BUTTON_USER)) {

        }
    }
}

// -----------------------------------------------------------

/**
 * @brief  Prepare USART3 to function as a debug printing interface
 * @param  None
 * @retval None
 */
void retarget_printf() {
    // Enable peripheral clocks
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_USART3_FORCE_RESET();
    __HAL_RCC_USART3_RELEASE_RESET();

    // Initialize pins as USART3 Tx and Rx
    GPIO_InitTypeDef GPIO_InitStructure;

    // TX
    GPIO_InitStructure.Pin = GPIO_PIN_8;
    GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.Alternate = GPIO_AF7_USART3;
    GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);

    // RX
    GPIO_InitStructure.Pin = GPIO_PIN_9;
    GPIO_InitStructure.Mode = GPIO_MODE_AF_OD;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);

    // Initialize USART3
    shUART3.Instance = USART3;
    shUART3.Init.BaudRate = 115200; //
    shUART3.Init.Mode = UART_MODE_TX_RX;
    shUART3.Init.Parity = UART_PARITY_NONE;
    shUART3.Init.WordLength = UART_WORDLENGTH_8B;
    shUART3.Init.StopBits = UART_STOPBITS_1;
    shUART3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    shUART3.Init.OverSampling = UART_OVERSAMPLING_16;
    shUART3.Init.ClockPrescaler = UART_PRESCALER_DIV16;
    //huart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(&shUART3);

    USART3->CR3 |= USART_CR3_OVRDIS;

    // Initialize retargeting
    RetargetInit(&shUART3);

    // set fallback output device
    RetargetSetFallbackOutput(output_usart);

    // enable DMA interrupts
    //HAL_NVIC_SetPriority(USART3_IRQn, 10, 10);
    //HAL_NVIC_EnableIRQ(USART3_IRQn);
}

// hardware initialization
void hw_init() {
    retarget_printf(); // retarget printf to USART3
}

/**
 * @brief  Main program
 * @param  None
 * @retval None
 */
int main(void) {
    /* Turn off MPU */
    HAL_MPU_Disable();

    /* STM32H7xx HAL library initialization:
     - Configure the TIM6 to generate an interrupt each 1 msec
     - Set NVIC Group Priority to 4
     - Low Level Initialization
     */
    HAL_Init();

    /* Configure the system clock to 400 MHz */
    SystemClock_Config();

    /* Configure the MPU attributes as Device memory for ETH DMA descriptors */
    MPU_Config();

    /* Further hardware initialization */
    hw_init();

    /* Enable the MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

    /* Enable the CPU Cache */
    CPU_CACHE_Enable();

    /* FIXME Configure the LEDs ...*/
    BSP_Config();

    /* Init thread */
    osThreadDef(Start, StartThread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE * 4);
    osThreadCreate(osThread(Start), NULL);

    MSG("\033[2J\033[H");
    MSG("Starting scheduler!\r\n");
    MSG("CPU freq. = %u Hz\n", HAL_RCC_GetSysClockFreq());

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */
    for (;;)
        ;
}

static void BoardThread(void const *argument);

#include "timersync/timersync.h"

/**
 * @brief  Start Thread
 * @param  argument not used
 * @retval None
 */
static void StartThread(void const *argument) {
    //pm_init();

    /* Create tcp_ip stack thread */
    tcpip_init(NULL, NULL);

    /* Initialize the LwIP stack */
    Netif_Config();

    /* register CLI task*/
    reg_task_cli();

    /* register board thread */
    reg_board_task();

    /* register task for PTP management */
    reg_task_eth();

    /* initialize timer synchronization */
    timersync_init();

    for (;;) {
        /* Delete the Init Thread */
        osThreadTerminate(NULL);
    }
}

/**
 * @brief  Initializes the lwIP stack
 * @param  None
 * @retval None
 */
static void Netif_Config(void) {
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

#if LWIP_DHCP
    ip_addr_set_zero_ip4(&ipaddr);
    ip_addr_set_zero_ip4(&netmask);
    ip_addr_set_zero_ip4(&gw);
#else
  IP_ADDR4(&ipaddr,IP_ADDR0,IP_ADDR1,IP_ADDR2,IP_ADDR3);
  IP_ADDR4(&netmask,NETMASK_ADDR0,NETMASK_ADDR1,NETMASK_ADDR2,NETMASK_ADDR3);
  IP_ADDR4(&gw,GW_ADDR0,GW_ADDR1,GW_ADDR2,GW_ADDR3);
#endif /* LWIP_DHCP */

    /* add the network interface */
    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

    /*  Registers the default network interface. */
    netif_set_default(&gnetif);

    ethernet_link_status_updated(&gnetif);

#if LWIP_NETIF_LINK_CALLBACK 
    netif_set_link_callback(&gnetif, ethernet_link_status_updated);

    osThreadDef(EthLink, ethernet_link_thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE *2);
    osThreadCreate(osThread(EthLink), &gnetif);
#endif   

#if LWIP_DHCP
    /* Start DHCPClient */
    osThreadDef(DHCP, DHCP_Thread, osPriorityBelowNormal, 0, configMINIMAL_STACK_SIZE * 2);
    osThreadCreate(osThread(DHCP), &gnetif);
#endif 
}

/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL1 (HSE BYPASS)
 *            SYSCLK(Hz)                     = 400000000 (CPU Clock)
 *            HCLK(Hz)                       = 200000000 (AXI and AHBs Clock)
 *            AHB Prescaler                  = 2
 *            D1 APB3 Prescaler              = 2 (APB3 Clock  100MHz)
 *            D2 APB1 Prescaler              = 2 (APB1 Clock  100MHz)
 *            D2 APB2 Prescaler              = 2 (APB2 Clock  100MHz)
 *            D3 APB4 Prescaler              = 2 (APB4 Clock  100MHz)
 *            HSE Frequency(Hz)              = 16000000
 *            PLL_M                          = 8
 *            PLL_N                          = 400
 *            PLL_P                          = 2
 *            PLL_Q                          = 4
 *            PLL_R                          = 2
 *            VDD(V)                         = 3.3
 *            Flash Latency(WS)              = 4
 *
 *            PLL2 is used for SAI 98.304MHz generation
 * @param  None
 * @retval None
 */

static void SystemClock_Config(void) {
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    HAL_StatusTypeDef ret = HAL_OK;

    /*!< Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    /* Enable D2 domain SRAM3 Clock (0x30040000 AXI)*/
    __HAL_RCC_D2SRAM3_CLK_ENABLE();

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;

#if OSC_TYPE == OSC_XTAL
    RCC_OscInitStruct.HSEState = RCC_HSE_ON; // HSE ON
#elif OSC_TYPE == OSC_EXT
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS; // HSE BYPASS ON
#endif
    RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
    RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLM = HSE_VALUE / 2000000; // 16/10MHz -> 2MHz
    RCC_OscInitStruct.PLL.PLLN = 400;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;

    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (ret != HAL_OK) {
        while (1)
            ;
    }

    /* Select PLL as system clock source and configure  bus clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
    RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1);

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
    if (ret != HAL_OK) {
        while (1)
            ;
    }

    /*activate CSI clock mandatory for I/O Compensation Cell*/
    __HAL_RCC_CSI_ENABLE();

    /* Enable SYSCFG clock mandatory for I/O Compensation Cell */
    __HAL_RCC_SYSCFG_CLK_ENABLE()
                ;

    /* Enables the I/O Compensation Cell */
    HAL_EnableCompensationCell();

    // get existing settings
    RCC_PeriphCLKInitTypeDef perClkInit;
    HAL_RCCEx_GetPeriphCLKConfig(&perClkInit);

    /* Initialize PLL2 */
    perClkInit.PLL2.PLL2M = HSE_VALUE / 2000000; // 16/10MHz -> 2MHz
    perClkInit.PLL2.PLL2N = 98; // 2MHz -> 196.608MHz
    perClkInit.PLL2.PLL2FRACN = 2490; // ...
    perClkInit.PLL2.PLL2P = 2; // 196.608MHz -> 98.304MHz

    // don't need these outputs, but don't leave them zero
    perClkInit.PLL2.PLL2Q = 10;
    perClkInit.PLL2.PLL2R = 10;

    perClkInit.PLL2.PLL2RGE = RCC_PLL1VCIRANGE_1; // Frequency range 1
    perClkInit.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM; // Medium-range PLL

    /* Set SAI clock source pll2_p_ck */
    perClkInit.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL2; // modify value for SAI1
    HAL_RCCEx_PeriphCLKConfig(&perClkInit); // save settings
}

/**
 * @brief  Configure the MPU attributes
 * @param  None
 * @retval None
 */
static void MPU_Config(void) {
    MPU_Region_InitTypeDef MPU_InitStruct;

    /* Configure the MPU attributes as Device not cacheable
     for ETH DMA descriptors */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = 0x30040000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_256B;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /* Configure the MPU attributes as Normal Non Cacheable
     for LwIP RAM heap which contains the Tx buffers */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = 0x30044000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_16KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    SCB_InvalidateDCache();
}

/**
 * @brief  CPU L1-Cache enable.
 * @param  None
 * @retval None
 */
static void CPU_CACHE_Enable(void) {
    /* Enable I-Cache */
    SCB_EnableICache();

    /* Enable D-Cache */
    //SCB_EnableDCache();
}

#define ETHERNET_HEADER_LENGTH (14)

err_t hook_unknown_ethertype(struct pbuf *pbuf, struct netif *netif) {
    // aquire ethertype
    uint16_t etherType = 0;
    memcpy(&etherType, ((uint8_t*) pbuf->payload) + 12, 2);
    etherType = ntohs(etherType);
    if (etherType == ETHERTYPE_PTP) {
        // verify Ethernet address
        if (!memcmp(PTP_ETHERNET_PRIMARY, pbuf->payload, 6) || !memcmp(PTP_ETHERNET_PEER_DELAY, pbuf->payload, 6)) { //
            ptp_enqueue_msg(((uint8_t*) pbuf->payload) + ETHERNET_HEADER_LENGTH, pbuf->len - ETHERNET_HEADER_LENGTH, pbuf->time_s, pbuf->time_ns, PTP_TP_802_3);
            pbuf_free(pbuf);
            return ERR_OK;
        }
    }

    return 1;
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
