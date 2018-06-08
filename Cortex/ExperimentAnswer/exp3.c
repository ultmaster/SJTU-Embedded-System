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
void S800_UART_Init(void);
//systick software counter define
volatile uint16_t systick_10ms_couter, systick_100ms_couter;
uint32_t systick_1s_counter, systick_1ms_counter;
volatile uint8_t systick_10ms_status, systick_100ms_status;

volatile uint8_t result, cnt, key_value, gpio_status;
volatile uint8_t rightshift = 0x01;
uint32_t ui32SysClock;
uint8_t seg7[] = { 0x3f, 0x06, 0x5b, 0x4f,
                   0x66, 0x6d, 0x7d, 0x07,
                   0x7f, 0x6f, 0x77, 0x7c,
                   0x58, 0x5e, 0x079, 0x71,
                   0x5c, 0x40 };
uint8_t uart_receive_char;

void t4_handler() {
	int32_t uart0_int_status;
    int len_message;
	char message[200];
	char responseMessage[200];
    uart0_int_status = UARTIntStatus(UART0_BASE, true);
	UARTIntClear(UART0_BASE, uart0_int_status);
    if (!uart0_int_status) return;
	
	len_message = UARTStringGet(message);
	memset(responseMessage, 0, sizeof responseMessage);
    for (int i = 0; i < len_message; ++i)
        if (message[i] >= 'a' && message[i] <= 'z')
            message[i] += 'A' - 'a';
	if (strcmp(message, "AT+CLASS") == 0)
		sprintf(responseMessage, "CLASS2018");
	else if (strcmp(message, "AT+STUDENTCODE") == 0)
		sprintf(responseMessage, "CODE716030210014");
    // sprintf(responseMessage, "%d %x\n", len_message, uart0_int_status);
	// UARTStringPutNonBlocking(message);
	UARTStringPutNonBlocking(responseMessage);
}

void t4_config() {
	UARTIntRegister(UART0_BASE, t4_handler);
}

uint8_t led_numbers[8];
uint32_t general_clock;
uint8_t current_display_loc;

void update_led_numbers() {
    int hours = (general_clock / 3600) % 24,
        mins = (general_clock / 60) % 60,
        secs = general_clock % 60;
    led_numbers[0] = hours / 10;
    led_numbers[1] = hours % 10;
    led_numbers[3] = mins / 10;
    led_numbers[4] = mins % 10;
    led_numbers[6] = secs / 10;
    led_numbers[7] = secs % 10;
}

void formatted_time() {
    char msg[200];
    sprintf(msg, "TIME%02d:%02d:%02d", general_clock / 3600, general_clock / 60 % 60, general_clock % 60);
    UARTStringPut(msg);
}

int two_digit_num(char* s, int i) {
    return (s[i] - '0') * 10 + s[i + 1] - '0';
}

void t5_handler() {
	int32_t uart0_int_status;
    int hour, min, sec, t;
	char message[200];
    uart0_int_status = UARTIntStatus(UART0_BASE, true);
	UARTIntClear(UART0_BASE, uart0_int_status);
    if (!uart0_int_status) return;
	
	UARTStringGet(message);
    if (strncmp(message, "SET", 3) == 0) {
        hour = two_digit_num(message, 3);
        min = two_digit_num(message, 6);
        sec = two_digit_num(message, 9);
        general_clock = (hour * 3600 + min * 60 + sec) % 86400;
        update_led_numbers();
        formatted_time();
    } else if (strncmp(message, "INC", 3) == 0) {
        hour = two_digit_num(message, 3);
        min = two_digit_num(message, 6);
        sec = two_digit_num(message, 9);
        general_clock = (general_clock + hour * 3600 + min * 60 + sec) % 86400;
        update_led_numbers();
        formatted_time();
    } else if (strcmp(message, "GETTIME") == 0) {
        formatted_time();
    }
}

void t5_systick()
{
    if (systick_1s_counter != 0)
        systick_1s_counter--;
    else {
        general_clock = (general_clock + 1) % 86400;
        update_led_numbers();
        systick_1s_counter = SYSTICK_FREQUENCY;
    }
    
    if (systick_1ms_counter != 0)
        systick_1ms_counter--;
    else {
        systick_1ms_counter = SYSTICK_FREQUENCY / 500;
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[led_numbers[current_display_loc]]);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 1 << current_display_loc);
        current_display_loc = (current_display_loc + 1) % 8;
    }
}

void t5(int good) {
    IntPriorityGroupingSet(3);
    if (good) {
        IntPrioritySet(FAULT_SYSTICK, 0x00);
        IntPrioritySet(INT_UART0, 0x80);
    } else {
        IntPrioritySet(FAULT_SYSTICK, 0x80);
        IntPrioritySet(INT_UART0, 0x00);
    }
    
    SysTickIntRegister(t5_systick);
    SysTickEnable();
    SysTickIntEnable();
    
    UARTIntRegister(UART0_BASE, t5_handler);
    memset(led_numbers, 17, sizeof led_numbers);
}

const char* MONTH[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                       "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

void q3_handler() {
	int32_t uart0_int_status;
    int len_message;
	char message[200], month[200];
    int offset = 0, sign;
	char responseMessage[200];
    uart0_int_status = UARTIntStatus(UART0_BASE, true);
	UARTIntClear(UART0_BASE, uart0_int_status);
    if (!uart0_int_status) return;
	
    len_message = UARTStringGet(message);
	memset(responseMessage, 0, sizeof responseMessage);
    memset(month, 0, sizeof month);
    int i = 0;
    for (i = 0; i < len_message; ++i) {
        if (message[i] == '+' || message[i] == '-')
            break;
        month[i] = message[i];
    }
    if (message[i] == '+') sign = 1;
    else sign = -1;
    for (i = i + 1; i < len_message; ++i)
        offset = offset * 10 + message[i] - '0';
    if (sign < 0) offset = (12 - offset % 12) % 12;
    for (i = 0; i < 12; ++i)
        if (strcmp(MONTH[i], month) == 0) {
            offset += i;
            break;
        }
    offset %= 12;
	UARTStringPutNonBlocking(MONTH[offset]);
}

void q3_config() {
	UARTIntRegister(UART0_BASE, q3_handler);
}

void q4_handler() {
	int32_t uart0_int_status;
    int h1, m1, h2, m2, tot;
	char msg[200], resp[200], symbol;
    uart0_int_status = UARTIntStatus(UART0_BASE, true);
	UARTIntClear(UART0_BASE, uart0_int_status);
    if (!uart0_int_status) return;
    
    UARTStringGet(msg);
    if (sscanf(msg, "%d:%d%c%d:%d", &h1, &m1, &symbol, &h2, &m2) == 5) {
        tot = h1 * 60 + m1 + (symbol == '+' ? 1 : -1) * (h2 * 60 + m2);
        sprintf(resp, "%02d:%02d", tot / 60, tot % 60);
    }

	UARTStringPutNonBlocking(resp);
}

void q4_config() {
	UARTIntRegister(UART0_BASE, q4_handler);
}

void switch_part() {
    // t4_config();
    // t5(1);
    // t5(0);
    // q3_config();
    q4_config();
}

int main(void)
{
    volatile uint16_t i2c_flash_cnt, gpio_flash_cnt;
    //use internal 16M oscillator, PIOSC
    //ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ |SYSCTL_OSC_INT |SYSCTL_USE_OSC), 16000000);
    //ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ |SYSCTL_OSC_INT |SYSCTL_USE_OSC), 8000000);
    //use external 25M oscillator, MOSC
    //ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN |SYSCTL_USE_OSC), 25000000);

    //use external 25M oscillator and PLL to 120M
    //ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN | SYSCTL_USE_PLL |SYSCTL_CFG_VCO_480), 120000000);;
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ | SYSCTL_OSC_INT | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 20000000);

    SysTickPeriodSet(ui32SysClock / SYSTICK_FREQUENCY);

    S800_GPIO_Init();
    S800_I2C0_Init();
    S800_UART_Init();

    IntEnable(INT_UART0);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT); //Enable UART0 RX,TX interrupt
    IntMasterEnable();
	
    switch_part();

    while (1);
}

void Delay(uint32_t value)
{
    uint32_t ui32Loop;
    for (ui32Loop = 0; ui32Loop < value; ui32Loop++);
}

void UARTStringPut(const char* cMessage)
{
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);
    while (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_1) == 0);
    while (*cMessage != '\0')
        UARTCharPut(UART0_BASE, *(cMessage++));
    Delay(10000);
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
}
void UARTStringPutNonBlocking(const char* cMessage)
{
    while (*cMessage != '\0')
        UARTCharPutNonBlocking(UART0_BASE, *(cMessage++));
}
int UARTStringGet(char* cMessage)
{
	int t = 0;
    while (UARTCharsAvail(UART0_BASE)) {
        while (UARTCharsAvail(UART0_BASE))
            cMessage[t++] = UARTCharGet(UART0_BASE);
        Delay(10000);
    }
    cMessage[t] = 0;
	return t;
}

void S800_UART_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA); //Enable PortA
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA))
        ; //Wait for the GPIO moduleA ready

    GPIOPinConfigure(GPIO_PA0_U0RX); // Set GPIO A0 and A1 as UART pins.
    GPIOPinConfigure(GPIO_PA1_U0TX);

    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure the UART for 115,200, 8-N-1 operation.
    UARTConfigSetExpClk(UART0_BASE, ui32SysClock, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    UARTStringPut((uint8_t*)"\r\nHello, world!\r\n");
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

    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0); //Set PF0 as Output pin
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0); //Set PN0 as Output pin
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1); //Set PN1 as Output pin

    GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1); //Set the PJ0,PJ1 as input pin
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
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

/*
	Corresponding to the startup_TM4C129.s vector table systick interrupt program name
*/
void SysTick_Handler(void)
{
    if (systick_1s_counter != 0)
        systick_1s_counter--;
    else {
        general_clock = (general_clock + 1) % 86400;
        update_led_numbers();
        systick_1s_counter = SYSTICK_FREQUENCY;
    }
    
    if (systick_100ms_couter != 0)
        systick_100ms_couter--;
    else {
        systick_100ms_couter = SYSTICK_FREQUENCY / 10;
        systick_100ms_status = 1;
    }

    if (systick_10ms_couter != 0)
        systick_10ms_couter--;
    else {
        systick_10ms_couter = SYSTICK_FREQUENCY / 100;
        systick_10ms_status = 1;
    }
    if (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0) {
        systick_100ms_status = systick_10ms_status = 0;
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
    }
    else
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);
}

/*
	Corresponding to the startup_TM4C129.s vector table UART0_Handler interrupt program name
*/
void UART0_Handler(void)
{
	#define MAXN 200
	char s[MAXN]; int sn = 0;
	int32_t uart0_int_status;
    uart0_int_status = UARTIntStatus(UART0_BASE, true); // Get the interrrupt status.
	
	memset(s, 0, sizeof s);

    UARTIntClear(UART0_BASE, uart0_int_status); //Clear the asserted interrupts

    while (UARTCharsAvail(UART0_BASE)) // Loop while there are characters in the receive FIFO.
		s[sn++] = UARTCharGetNonBlocking(UART0_BASE);

    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
}
