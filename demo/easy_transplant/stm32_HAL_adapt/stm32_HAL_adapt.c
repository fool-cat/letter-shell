/**
 * @file stm32_HAL_adapt.c
 * @author 独霸一方 (2696652257@qq.com)
 * @brief //> stm32 HAL库适配letter shell
 * @version 1.0
 * @date 2024-08-07
 *
 * @copyright Copyright (c) 2024
 *
 */

// clang-format off
//> 必须对接实现的API,哪怕是空函数也要定义一份(可以通过配置宏来预处理关掉,但是还是建议实现,哪怕是空函数)

/*

// shell锁实现函数
int shell_lock(struct shell_def *shell) {};
int shell_unlock(struct shell_def *shell) {};

// log锁实现函数
int log_lock(struct log_def *log) {};
int log_unlock(struct log_def *log) {};

// shell初始化之前的初始化函数主要是为了初始化硬件接口等
void user_init_before_shell(void) {};

*/
// clang-format on

#include "stm32_HAL_adapt.h"

#include "../shell_all.h"

#if (RTOS_MODE == RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "semphr.h"
static SemaphoreHandle_t _shell_mutex; // 互斥锁,防止输出混乱
static SemaphoreHandle_t _log_mutex;   // 互斥锁,防止输出混乱
#endif

// 写入是否可以阻塞,默认不可以阻塞,一般判断是否在中断中来决定是否可以阻塞
#ifndef SHELL_WRITE_CAN_BLOCK
#define SHELL_WRITE_CAN_BLOCK() (false)
#endif // !SHELL_WRITE_CAN_BLOCK

// 如果有操作系统,可以使用操作系统提供的递归互斥锁
int shell_lock(struct shell_def *shell)
{
    if (SHELL_WRITE_CAN_BLOCK())
    { // 操作系统的加锁
#if (RTOS_MODE == RTOS_FREERTOS)
        xSemaphoreTakeRecursive(_shell_mutex, portMAX_DELAY);
#endif
    }
    else
    {
        // 处于不能阻塞的地方,一般认为是中断,直接不管或者必须使用支持嵌套的原子加锁,下面的几个API类似
    }

    return 0;
}

int shell_unlock(struct shell_def *shell)
{
    if (SHELL_WRITE_CAN_BLOCK())
    { // 操作系统的解锁
#if (RTOS_MODE == RTOS_FREERTOS)
        xSemaphoreGiveRecursive(_shell_mutex);
#endif
    }
    else
    {
    }

    return 0;
}

int log_lock(struct log_def *log)
{
    if (SHELL_WRITE_CAN_BLOCK())
    { // 操作系统的加锁
#if (RTOS_MODE == RTOS_FREERTOS)
        xSemaphoreTakeRecursive(_log_mutex, portMAX_DELAY);
#endif
    }
    else
    {
    }

    return 0;
}

int log_unlock(struct log_def *log)
{
    if (SHELL_WRITE_CAN_BLOCK())
    { // 操作系统的解锁
#if (RTOS_MODE == RTOS_FREERTOS)
        xSemaphoreGiveRecursive(_log_mutex);
#endif
    }
    else
    {
    }

    return 0;
}

// shell使用的硬件初始化,使用cubemx开启串口,然后使能对应的串口中断,DMA发送与接收中断
void user_init_before_shell(void)
{
    // 由于初始化已经由CUBEMX处理好了,这里什么都不需要做

#if (RTOS_MODE == RTOS_FREERTOS)
    _shell_mutex = xSemaphoreCreateMutex();
    _log_mutex = xSemaphoreCreateMutex();
#endif
}

//+******************************** 平台的中断回调 ***************************************/
//! 如果有自己其他串口的中断回调,请自行按照这个格式添加

#if 1

#if (PLATFORM_MODE == PLATFORM_MODE_DMA)

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) // 串口接收中断
{
    UNUSED(huart);
    UNUSED(Size);

    if (huart->Instance == SHELL_UART_ADDR->Instance)
    {
        port_rx_end(Size);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) // 串口发送中断
{
    UNUSED(huart);

    if (huart->Instance == SHELL_UART_ADDR->Instance)
    {
        port_tx_end(-1); // 传输默认是传输完了上一次的,传入负数让其自行处理
    }
}

#elif (PLATFORM_MODE == PLATFORM_MODE_IT)

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == SHELL_UART_ADDR->Instance)
    {
        port_rx_end(1); // 中断接收的时候是单字节接收,因此实际接收到1个字节就会进入此中断
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) // 串口发送中断
{
    UNUSED(huart);

    if (huart->Instance == SHELL_UART_ADDR->Instance)
    {
        port_tx_end(-1); // 传输默认是传输完了上一次的,传入负数让其自行处理
    }
}

#endif

#endif

//+******************************** 导出一些可能常用的命令 ***************************************/
#if 1

int Reboot()
{
    HAL_NVIC_SystemReset(); // 单片机重启
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), Reboot, Reboot, reboot the mcu);

#include <stdio.h>
int date()
{
    Shell *shell = shellGetCurrent();

    if (!shell)
        return 0;

    int64_t ticks = SHELL_GET_TICK();

#if SHELL_USING_LOCK == 1
    shell->lock(shell);
#endif

    char str[150];
    snprintf(str, sizeof(str), "current time: %lld\r\n", ticks);
    shellWriteString(shell, str);

    // 换算为天`时`分`秒`毫秒
    // snprintf(str, sizeof(str), "current time: %lld 天 %02lld 时 %02lld 分 %02lld 秒 %03lld 毫秒\r\n", ticks / (1000 * 60 * 60 * 24), (ticks / (1000 * 60 * 60)) % 24, (ticks / (1000 * 60)) % 60, (ticks / 1000) % 60, ticks % 1000);
    snprintf(str, sizeof(str), "current time: %lld days %02lld hours %02lld minutes %02lld seconds %03lld milliseconds\r\n", ticks / (1000 * 60 * 60 * 24), (ticks / (1000 * 60 * 60)) % 24, (ticks / (1000 * 60)) % 60, (ticks / 1000) % 60, ticks % 1000);
    shellWriteString(shell, str);

#if SHELL_USING_LOCK == 1
    shell->unlock(shell);
#endif

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), date, date, current time);

#endif
