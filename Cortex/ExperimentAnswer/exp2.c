#include <stdint.h>
#include <stdbool.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_i2c.h"
#include "hw_types.h"
#include "i2c.h"
#include "pin_map.h"
#include "sysctl.h"
#include "interrupt.h"
#include "systick.h"
#include "hw_ints.h"

// I2C GPIO chip address and resigster define
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

volatile uint8_t result;
uint32_t ui32SysClock;
bool paused;
uint8_t seg7[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x58, 0x5e, 0x079, 0x71,
                  0x5c};
void PortJ_IntHandler(void);

void Delay(uint32_t value) {
    uint32_t ui32Loop;
    for (ui32Loop = 0; ui32Loop < value; ui32Loop++);
}

uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData) {
    uint8_t rop;
    while (I2CMasterBusy(I2C0_BASE)); // wait if busy
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false); // false is write, true is read

    I2CMasterDataPut(I2C0_BASE, RegAddr); // write device register address
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START); // repeat writing
    while (I2CMasterBusy(I2C0_BASE));

    rop = (uint8_t) I2CMasterErr(I2C0_BASE); // debug purpose

    I2CMasterDataPut(I2C0_BASE, WriteData);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH); // repeat writing
    while (I2CMasterBusy(I2C0_BASE)) {};

    rop = (uint8_t) I2CMasterErr(I2C0_BASE); // debug purpose

    return rop;
}

uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr) {
    uint8_t value, rop;
    while (I2CMasterBusy(I2C0_BASE)) {};
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
    I2CMasterDataPut(I2C0_BASE, RegAddr);

    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_SEND); // single write
    while (I2CMasterBusBusy(I2C0_BASE));
    rop = (uint8_t) I2CMasterErr(I2C0_BASE);
    Delay(1);

    // receive data
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, true);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE); // single read
    while (I2CMasterBusBusy(I2C0_BASE));
    value = I2CMasterDataGet(I2C0_BASE); // get read data
    Delay(1);
    return value;
}

void S800_I2C0_Init(void) {
    uint8_t result;
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0); // init I2C
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB); // use I2C module 0, Pin: I2C0SCL--PB2?I2C0SDA--PB3
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

void S800_GPIO_Init(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); //Enable PortF
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF)); //Wait for the GPIO moduleF ready
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ); //Enable PortJ
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ)); //Wait for the GPIO moduleJ ready

    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1); //Set PF0 as Output pin
    GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1);//Set the PJ0,PJ1 as input pin
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void PortJ_IntHandler(void) {
	uint32_t ulStatus;
	ulStatus = GPIOIntStatus(GPIO_PORTJ_BASE, true);
	GPIOIntClear(GPIO_PORTJ_BASE, ulStatus);
    paused = ! paused;
}

int current_task;
uint32_t current_pf0, current_number;
void SysTick_IntHandler(void) {
    if (current_task == 0) {
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, current_pf0);
        if (current_pf0 == 0) current_pf0 = GPIO_PIN_0;
        else current_pf0 = 0;
    } else {
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[current_number + 1]); //write port 1
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, (uint8_t)(1 << current_number)); //write port 2
        result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(~(1 << current_number)));
        current_number = (current_number + 1) % 8;
    }
    current_task = (current_task + 1) % 2;
}

void block_handler(void) {
    uint32_t ulStatus;
	ulStatus = GPIOIntStatus(GPIO_PORTJ_BASE, true);
	GPIOIntClear(GPIO_PORTJ_BASE, ulStatus);
    
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0);
    //result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(~1));
    //ASSERT (result == 0);
    while (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0);
    //result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(0xff));
    //ASSERT (result == 0);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0);
}

void example_program() {
    while (1) {
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[2]); //write port 1
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, (uint8_t)(3)); //write port 2
        result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, 0x0);
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_PIN_0); // Turn on the PF0
        Delay(800000);
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0x0); // Turn off the PF0.
        Delay(800000);
    }
}

void test1() {
    int i;
    for (i = 0; ; i = (i + 1) % 8) {
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[i + 1]); //write port 1
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, (uint8_t)(1 << i)); //write port 2
        result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(~(1 << i)));
        Delay(800000);
    }
}

void test2() {
    int i;
    for (i = 0; ; i = (i + 1) % 8) {
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[i + 1]); //write port 1
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, (uint8_t)(1 << i)); //write port 2
        result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(~(1 << i)));
        while (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0) Delay(800000);
        Delay(800000);
    }
}

void test3_for1_handler() {
	result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[current_number + 1]); //write port 1
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, (uint8_t)(1 << current_number)); //write port 2
    result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(~(1 << current_number)));
    current_number = (current_number + 1) % 8;
}

void test3_for2_handler() {
	if (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0) return;
	result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[current_number + 1]); //write port 1
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, (uint8_t)(1 << current_number)); //write port 2
    result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(~(1 << current_number)));
    current_number = (current_number + 1) % 8;
}

void test3(int i) {
    SysTickEnable();
    SysTickIntEnable();
    SysTickPeriodSet(3000000);
    if (i == 1) SysTickIntRegister(test3_for1_handler);
	else SysTickIntRegister(test3_for2_handler);
	
    while (1);
}

void test4() {
    SysTickEnable();
    SysTickIntEnable();
    SysTickPeriodSet(1600000);
    SysTickIntRegister(SysTick_IntHandler);
    
    GPIOIntRegister(GPIO_PORTJ_BASE, block_handler);
    GPIOIntTypeSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_FALLING_EDGE);
    GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_0);
    
    while (1);
}

void question2() {
	int i;
    for (i = 0; ; i = (i + 1) % 8) {
        uint8_t j = (1 << i) + (1 << (i + 7) % 8);
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[i + 1]); //write port 1
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, j); //write port 2
        result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, ~j);
        Delay(800000);
    }
}

void question3() {
	int i, k;
    uint8_t j1, j2, j;
    for (i = 0; ; i = (i + 1) % 8) {
		j1 = (1 << i);
        j2 = (1 << (i + 7) % 8);
        j = j1 + j2;
        result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, ~j);
        for (k = 0; k < 20; ++k) {
            result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[i + 1]); //write port 1
            result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, j1); //write port 2
            Delay(20000);
            result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[(i + 7) % 8 + 1]); //write port 1
            result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, j2); //write port 2
            Delay(20000);
		}
    }
}

#define CLOCK_FREQ 16000000

float q4_period[] = {1, 2, 0.2, 0.5};
int q4_counter = 3;
void question4_speedup_handler(void) {
    uint32_t ulStatus;
	ulStatus = GPIOIntStatus(GPIO_PORTJ_BASE, true);
	GPIOIntClear(GPIO_PORTJ_BASE, ulStatus);
    q4_counter = (q4_counter + 1) % 4;
}

void question4() {
    int i;
    
    GPIOIntRegister(GPIO_PORTJ_BASE, question4_speedup_handler);
    GPIOIntTypeSet(GPIO_PORTJ_BASE, GPIO_PIN_0, GPIO_FALLING_EDGE);
    GPIOIntEnable(GPIO_PORTJ_BASE, GPIO_PIN_0);
    
    for (i = 0; ; i = (i + 1) % 8) {
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[i + 1]); //write port 1
        result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, (uint8_t)(1 << i)); //write port 2
        result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, (uint8_t)(~(1 << i)));
        Delay((uint32_t) (q4_period[q4_counter] * CLOCK_FREQ / 5));
    }
}


int main(void) {
    //use internal 16M oscillator, HSI
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ | SYSCTL_OSC_INT | SYSCTL_USE_OSC), CLOCK_FREQ);

    IntMasterEnable();
    
    S800_GPIO_Init();
    S800_I2C0_Init();
	
    question4();
	// test3(1);
    // test3(2);
	// test4();
}
