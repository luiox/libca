/**
 * @file resource_port.h
 * @author canrad (1517807724@qq.com)
 * @brief 资源ID定义
 * @version 0.1
 * @date 2025-08-03
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef RESOURCE_PORT_H
#define RESOURCE_PORT_H

// GPIO资源ID
#define GPIO_LED_INTERFACE_ID       0x00
#define GPIO_LED_HANDLE_ID          0x01
#define GPIO_BUTTON_INTERFACE_ID    0x02
#define GPIO_BUTTON_HANDLE_ID       0x03

// UART资源ID
#define UART1_INTERFACE_ID          0x10
#define UART1_HANDLE_ID             0x11
#define UART2_INTERFACE_ID          0x12
#define UART2_HANDLE_ID             0x13

// SPI资源ID
#define SPI1_INTERFACE_ID           0x20
#define SPI1_HANDLE_ID              0x21
#define SPI2_INTERFACE_ID           0x22
#define SPI2_HANDLE_ID              0x23

// I2C资源ID
#define I2C1_INTERFACE_ID           0x30
#define I2C1_HANDLE_ID              0x31
#define I2C2_INTERFACE_ID           0x32
#define I2C2_HANDLE_ID              0x33

// Timer资源ID
#define TIMER1_INTERFACE_ID         0x40
#define TIMER1_HANDLE_ID            0x41

// ADC资源ID
#define ADC1_INTERFACE_ID           0x50
#define ADC1_HANDLE_ID              0x51

// PWM资源ID
#define PWM1_INTERFACE_ID           0x60
#define PWM1_HANDLE_ID              0x61

#endif // RESOURCE_PORT_H