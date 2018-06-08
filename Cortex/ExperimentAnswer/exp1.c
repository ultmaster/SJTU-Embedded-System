#include <stdint.h>
#include <stdbool.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_types.h"
#include "pin_map.h"
#include "sysctl.h"
#include "i2c.h"

#define VERYFASTFLASHTIME (uint32_t)60000
#define FASTFLASHTIME (uint32_t)500000
#define SLOWFLASHTIME (uint32_t)4000000

uint32_t delay_time, key_value;

void Delay(uint32_t value) {
    uint32_t ui32Loop;
    for (ui32Loop = 0; ui32Loop < value; ui32Loop++) {
    }
}

void Clock_Init() {
    // task 2
    SysCtlClockFreqSet(SYSCTL_OSC_INT | SYSCTL_USE_OSC, 16000000); // use PIOSC
    // SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_OSC, 25000000); // use MOSC
    // SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 200000000); // use PLL
}

void task3() {
    while (1) {
        key_value = 0x0;
        if (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0)
            key_value |= GPIO_PIN_0;
        if (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_1) == 0)
            key_value |= GPIO_PIN_1;
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1, key_value);
        Delay(FASTFLASHTIME);
    }
}

void flash(uint32_t pin_number, uint32_t interval) {
    GPIOPinWrite(GPIO_PORTF_BASE, pin_number, pin_number); // Turn on the LED.
    Delay(interval);
    GPIOPinWrite(GPIO_PORTF_BASE, pin_number, 0x0); // Turn off the LED.
    Delay(interval);
}

void task4() {
    uint32_t last_val = 1;
    uint32_t counter = 0;
    while (1) {
        key_value = GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0);
        if (key_value == 0 && last_val) {
            // rising edge of the button
            counter = (counter + 1) % 4;
        }
        last_val = key_value;
        switch (counter) {
            case 0: break; // do nothing
            case 1: flash(GPIO_PIN_0, FASTFLASHTIME); break;
            case 2: break; // do nothing
            case 3: flash(GPIO_PIN_1, FASTFLASHTIME); break;
        }
    }
}

void S800_GPIO_Init() {
    Clock_Init();
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); //Enable PortF
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
        ; //Wait for the GPIO moduleF ready
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ); //Enable PortJ
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ))
        ; //Wait for the GPIO moduleJ ready

    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1); //Set PF0 as Output pin
    GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1); //Set the PJ0,PJ1 as input pin
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void default_loop() {
    while (1) {
        key_value = GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0); //read the PJ0 key value

        if (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_1) == 0)
            delay_time = VERYFASTFLASHTIME;
        else if (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0) //USR_SW1-PJ0 pressed
            delay_time = FASTFLASHTIME;
        else
            delay_time = SLOWFLASHTIME;

        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0); // Turn on the LED.
        Delay(delay_time);
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0); // Turn off the LED.
        Delay(delay_time);
    }
}

uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData){
	uint8_t rop;
	
	while(I2CMasterBusy(I2C0_BASE)){};

	I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
	I2CMasterDataPut(I2C0_BASE, RegAddr);
	I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
	while(I2CMasterBusy(I2C0_BASE)){};
	rop = (uint8_t)I2CMasterErr(I2C0_BASE);
	I2CMasterDataPut(I2C0_BASE, WriteData);
	I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
	while(I2CMasterBusy(I2C0_BASE)){};
	rop = (uint8_t)I2CMasterErr(I2C0_BASE);
	return rop;
}

#define TCA6424_I2CADDR 				0x22
#define PCA9557_I2CADDR				0x18

#define PCA9557_INPUT					0x00
#define PCA9557_OUTPUT				0x01
#define PCA9557_POLINVERT				0x02
#define PCA9557_CONFIG				0x03

#define TCA6424_CONFIG_PORT0				0x0c
#define TCA6424_CONFIG_PORT1				0x0d
#define TCA6424_CONFIG_PORT2				0x0e

#define TCA6424_INPUT_PORT0				0x00
#define TCA6424_INPUT_PORT1				0x01
#define TCA6424_INPUT_PORT2				0x02

#define TCA6424_OUTPUT_PORT0				0x04
#define TCA6424_OUTPUT_PORT1				0x05
#define TCA6424_OUTPUT_PORT2				0x06


void S800_I2C0_Init(void){
    uint32_t ui32SysClock; // Set the system clock to run from the PLL at 120 MHz.
    uint8_t result;
    ui32SysClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);
    
	SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	GPIOPinConfigure(GPIO_PB2_I2C0SCL);
  	GPIOPinConfigure(GPIO_PB3_I2C0SDA);
  	GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
  	GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);
	I2CMasterInitExpClk(I2C0_BASE, ui32SysClock, true); 	//config I2C0 400k
	I2CMasterEnable(I2C0_BASE);	
	result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_CONFIG_PORT0,0x0ff);		//config port 0 as input
	result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_CONFIG_PORT1,0x0);		//config port 1 as output
	result = I2C0_WriteByte(TCA6424_I2CADDR,TCA6424_CONFIG_PORT2,0x0);		//config port 2 as output 
	result = I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_CONFIG,0x00);		//config port as output
	result = I2C0_WriteByte(PCA9557_I2CADDR,PCA9557_OUTPUT,0x0ff);		//turn off the LED1-8	
}


int main() {
    S800_I2C0_Init();
    // default_loop();
    // task3();
    task4();
}

