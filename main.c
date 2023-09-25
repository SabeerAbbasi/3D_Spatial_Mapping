/**
 *	COMPENG 2DX3 Final Project
 *	Sabeer Abbasi - 400359910 - abbass28

 *
 *	PL[0-3] : Servo Controller IN[1-4]
 *	PB[2-3]	:	I2C (SDA & SCL)
 *	PH0			:	Push Button Input Active High (Start/Stop)
 *	PH1			:	Push Button Input Active High	(Rotate Back)
 */

#include <stdint.h>
#include "tm4c1294ncpdt.h"
#include "SysTick.h"
#include "PLL.h"
#include "onboardLEDs.h"
#include "uart.h"
#include "vl53l1x_api.h"

#define I2C_MCS_ACK             0x00000008  // Data Acknowledge Enable
#define I2C_MCS_DATACK          0x00000008  // Acknowledge Data
#define I2C_MCS_ADRACK          0x00000004  // Acknowledge Address
#define I2C_MCS_STOP            0x00000004  // Generate STOP
#define I2C_MCS_START           0x00000002  // Generate START
#define I2C_MCS_ERROR           0x00000002  // Error
#define I2C_MCS_RUN             0x00000001  // I2C Master Enable
#define I2C_MCS_BUSY            0x00000001  // I2C Busy
#define I2C_MCR_MFE             0x00000010  // I2C Master Function Enable

#define MAXRETRIES              5           // number of receive attempts before giving up

void PortL0L1L2L3_Init(void) {
	SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R10;
	while((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R10) == 0) {};
	GPIO_PORTL_DIR_R = 0b00001111;
	GPIO_PORTL_DEN_R = 0b00001111;
}

void PortH0H1_Init(void) {
	SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R7;
	while((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R7) == 0) {};
	GPIO_PORTH_DIR_R = 0b00000000;
	GPIO_PORTH_DEN_R = 0b00000011;
}

void I2C_Init(void){
  SYSCTL_RCGCI2C_R |= SYSCTL_RCGCI2C_R0;
  SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
  while((SYSCTL_PRGPIO_R&0x0002) == 0) {};
  GPIO_PORTB_AFSEL_R |= 0x0C;
  GPIO_PORTB_ODR_R |= 0x08;
  GPIO_PORTB_DEN_R |= 0x0C;                                                       
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & 0xFFFF00FF) + 0x00002200;
  I2C0_MCR_R = I2C_MCR_MFE;
  I2C0_MTPR_R = 0b0000000000000101000000000111011;   
}

void rotateMotorCW() {
	for (int i = 0; i < 16; i++) {
		GPIO_PORTL_DATA_R = 0b00001001;
		SysTick_Wait(150000);
		GPIO_PORTL_DATA_R = 0b00000011;
		SysTick_Wait(150000);
		GPIO_PORTL_DATA_R = 0b00000110;
		SysTick_Wait(150000);
		GPIO_PORTL_DATA_R = 0b00001100;
		SysTick_Wait(150000);
	}
}

void rotateMotorCCW() {
	for (int i = 0; i < 16; i++) {
		GPIO_PORTL_DATA_R = 0b00001100;
		SysTick_Wait(150000);
		GPIO_PORTL_DATA_R = 0b00000110;
		SysTick_Wait(150000);
		GPIO_PORTL_DATA_R = 0b00000011;
		SysTick_Wait(150000);
		GPIO_PORTL_DATA_R = 0b00001001;
		SysTick_Wait(150000);
	}
}

//address of the ToF sensor as an I2C slave peripheral
uint16_t dev = 0x29;
int status=0;

int main(void) {
	// PSYSDIV is set to 15 (480 MHz / (15 + 1) = 30 MHz)
	PLL_Init();
	// SysTick_Wait10ms is set to 300000 (300000 / 30 MHz = 10ms)
	SysTick_Init();

	onboardLEDs_Init();

	I2C_Init();
	UART_Init();

	PortL0L1L2L3_Init();
	PortH0H1_Init();

	uint8_t byteData;
	uint8_t dataReady;
	uint8_t sensorState = 0;
	uint8_t myByteArray[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	uint16_t distance;

	UART_printf("Wait!\r\n");

	while(sensorState == 0) {
		status = VL53L1X_BootState(dev, &sensorState);
		SysTick_Wait10ms(1);
  }

	//FlashAllLEDs();

	status = VL53L1X_ClearInterrupt(dev);

	status = VL53L1X_SensorInit(dev);
	Status_Check("SensorInit", status);

	// Print "start" to trigger visualizer to collect data
	sprintf(printf_buffer, "start\r\n");
	UART_printf(printf_buffer);

	//polling while loop
	while (1) {
		// PH0 triggers data collection
		if (GPIO_PORTH_DATA_R & 1) {
			//FlashAllLEDs();
			SysTick_Wait10ms(50);

			status = VL53L1X_StartRanging(dev);

			// Get 32 measurements per set 11.25 deg per set
			for (int i = 0; i < 32; i++) {
				while (dataReady == 0) {
					status = VL53L1X_CheckForDataReady(dev, &dataReady);
					VL53L1_WaitMs(dev, 5);
					FlashLED2(10); //flash led 1
				}
				dataReady = 0;
				//i2c data gets transfered
				status = VL53L1X_GetDistance(dev, &distance);

				rotateMotorCW();

				status = VL53L1X_ClearInterrupt(dev);

				sprintf(printf_buffer, "%u\r\n", distance);
				UART_printf(printf_buffer);

				FlashLED2(1);
				SysTick_Wait10ms(10);
				
				
				// Stop if the button is pressed again (with debounce).
				if (GPIO_PORTH_DATA_R & 1) {
					FlashLED1(10);
					while (GPIO_PORTH_DATA_R & 1) {
					}
					break;
				}
			}
			
			
// used for untangling wires
			VL53L1X_StopRanging(dev);
		// PH1 triggers reverse rotation
		} else if ((GPIO_PORTH_DATA_R >> 1) & 1) {
			for (int i = 0; i < 32; i++) {
				rotateMotorCCW();

				// Stop if the button is pressed again (with debounce).
				if ((GPIO_PORTH_DATA_R >> 1) & 1) {
					while ((GPIO_PORTH_DATA_R >> 1) & 1) {}
					break;
				}
			}
		}
	}
}
