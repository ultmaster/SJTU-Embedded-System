#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_i2c.h"
#include "hw_types.h"
#include "i2c.h"
#include "pin_map.h"
#include "sysctl.h"
#include "systick.h"
#include "interrupt.h"
#include "uart.h"
#include "hw_ints.h"

#define SYSTICK_FREQUENCY 1000 //1000hz

#define I2C_FLASHTIME 500 //500mS
#define GPIO_FLASHTIME 300 //300mS
//*****************************************************************************
//
//I2C GPIO chip address and resigster define
//
//*****************************************************************************
#define TCA6424_I2CADDR 0x22
#define PCA9557_I2CADDR 0x18

#define PCA9557_INPUT 0x00
#define PCA9557_OUTPUT 0x01
#define PCA9557_POLINVERT 0x02
#define PCA9557_CONFIG 0x03

#define TCA6424_CONFIG_PORT0 0x0c
#define TCA6424_CONFIG_PORT1 0x0d
#define TCA6424_CONFIG_PORT2 0x0e

#define TCA6424_INPUT_PORT0 0x00
#define TCA6424_INPUT_PORT1 0x01
#define TCA6424_INPUT_PORT2 0x02

#define TCA6424_OUTPUT_PORT0 0x04
#define TCA6424_OUTPUT_PORT1 0x05
#define TCA6424_OUTPUT_PORT2 0x06

void Delay(uint32_t value);
void S800_GPIO_Init(void);
uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData);
uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr);
void UARTStringPut(const char* cMessage);
void UARTStringPutNonBlocking(const char* cMessage);
int UARTStringGet(char* cMessage);
void S800_I2C0_Init(void);

volatile uint8_t result, cnt, key_value, gpio_status;
volatile uint8_t rightshift = 0x01;
uint32_t ui32SysClock;
uint8_t seg7[] = { 0x3f, 0x06, 0x5b, 0x4f,
                   0x66, 0x6d, 0x7d, 0x07,
                   0x7f, 0x6f, 0x77, 0x7c,
                   0x58, 0x5e, 0x079, 0x71,
                   0x5c, 0x40 };

uint8_t CCW[8] = {0x09,0x01,0x03,0x02,0x06,0x04,0x0c,0x08};
uint8_t CW[8] = {0x08,0x0c,0x04,0x06,0x02,0x03,0x01,0x09}; 

int  change_angle=64;  //change the parameter to change the angle of the stepper

void Motor_CCW()    //the steper move 360/64 angle at CouterClockwise 
{
  for(int i = 0; i < 8; i++)
  
    for(int j = 0; j < 8; j++)
    {
      GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, CCW[j]);
      Delay(5000);
    }    
}
/*
void Motor_CW()  //the steper move 360/64 angle at Clockwise
{
  for(int i = 0; i < 8; i++)
  
    for(int j = 0; j < 8; j++)
    {
    if(digitalRead(stop_key)==0)
      {
      PORTB =0xf0;
      break;
      } 
      PORTB = CW[j];
      delayMicroseconds(1150);
    }
}
*/

void loop()
{
 Motor_CCW();  //make the stepper to anticlockwise rotate
// Motor_LR(); //make the stepper to clockwise rotate
}

int main(void)
{
    volatile uint16_t i2c_flash_cnt, gpio_flash_cnt;
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ | SYSCTL_OSC_INT | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 20000000);

    SysTickPeriodSet(ui32SysClock / SYSTICK_FREQUENCY);

    S800_GPIO_Init();
    S800_I2C0_Init();

    while (1) {
        loop();
    }
}

void Delay(uint32_t value) {
	uint32_t ui32Loop;
	for (ui32Loop = 0; ui32Loop < value; ui32Loop++);
}

void S800_GPIO_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); //Enable PortF
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
        ; //Wait for the GPIO moduleF ready
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ); //Enable PortJ
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ))
        ; //Wait for the GPIO moduleJ ready
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION); //Enable PortN
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION))
        ; //Wait for the GPIO moduleN ready
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK); //Enable PortN
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOK))
        ; //Wait for the GPIO moduleN ready

    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4); //Set PF0 as Output pin
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1); //Set PN1 as Output pin
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_5);
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_4);

    GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1); //Set the PJ0,PJ1 as input pin
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    // GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_4, GPIO_PIN_4);
    // GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_5, GPIO_PIN_5);  // beep
}



void S800_I2C0_Init(void)
{
    uint8_t result;
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

    I2CMasterInitExpClk(I2C0_BASE, ui32SysClock, true); //config I2C0 400k
    I2CMasterEnable(I2C0_BASE);

    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT0, 0x0ff); //config port 0 as input
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT1, 0x0); //config port 1 as output
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT2, 0x0); //config port 2 as output

    result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_CONFIG, 0x00); //config port as output
    result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, 0x0ff); //turn off the LED1-8
}

uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData)
{
    uint8_t rop;
    while (I2CMasterBusy(I2C0_BASE)) {
    };
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
    I2CMasterDataPut(I2C0_BASE, RegAddr);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    while (I2CMasterBusy(I2C0_BASE)) {
    };
    rop = (uint8_t)I2CMasterErr(I2C0_BASE);

    I2CMasterDataPut(I2C0_BASE, WriteData);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
    while (I2CMasterBusy(I2C0_BASE)) {
    };

    rop = (uint8_t)I2CMasterErr(I2C0_BASE);
    return rop;
}

uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr)
{
    uint8_t value, rop;
    while (I2CMasterBusy(I2C0_BASE)) {
    };
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
    I2CMasterDataPut(I2C0_BASE, RegAddr);
    //	I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_SEND);
    while (I2CMasterBusBusy(I2C0_BASE))
        ;
    rop = (uint8_t)I2CMasterErr(I2C0_BASE);
    Delay(1);
    //receive data
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, true);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
    while (I2CMasterBusBusy(I2C0_BASE))
        ;
    value = I2CMasterDataGet(I2C0_BASE);
    Delay(1);
    return value;
}
