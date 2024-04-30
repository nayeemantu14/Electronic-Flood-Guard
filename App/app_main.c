// This program controls a flood guard system using an STM32 microcontroller.

// Including necessary libraries
#include "main.h"          					// Include main header file
#include <stdio.h>         					// Include standard input/output library
#include <string.h>        					// Include string manipulation library
#include <stdbool.h>						// Include boolean data type

// External peripheral handlers declaration
extern ADC_HandleTypeDef hadc1;      		// Declare ADC handler
extern TIM_HandleTypeDef htim3;      		// Declare Timer 3 handler
extern TIM_HandleTypeDef htim16;     		// Declare Timer 16 handler
extern UART_HandleTypeDef huart2;    		// Declare UART handler

// Global variable declaration
char message[40];                     		// Buffer to store messages

uint8_t wupFlag = 0;  						// Initialize wake-up flag
static uint8_t Low_battery;					// Initialize low battery flag

volatile static uint8_t valve_open;			// Initialize valve open flag
volatile static uint8_t floodFlag = 0;    		// Initialize flood flag
volatile static uint8_t buttonState = 0;  	// Initialize button state
volatile static uint32_t holdTime = 0;    	// Initialize button hold time

uint32_t last_batt_time = 0;              	// Initialize last battery reading time
uint32_t sleep_time = 0; 					// Initialize sleep time
uint32_t alert_time = 0;					// Initialize alert time

// Function prototypes
void openValve();                    		// Function prototype for opening the valve
void closeValve();                    		// Function prototype for closing the valve
void alert(void);							// Function prototype for activating the buzzer and warning LED
uint16_t measureBattery(void);        		// Function prototype for measuring battery voltage
void monitorBattery(void);					// Function prototype for monitor battery voltage
void statusled(void);
void batteryled(void);						// Function prototype for activating battery LED
void console(char *log);              		// Function prototype for transmitting messages via UART

// Main application function
void app_main(void)
{
	// Initialize message buffer with default message
	strcpy(message, "EFloodGuard\r\n");
	// Send initialization message
	console(message);

	// Check if the flood flag is set
	if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET)
	{
		floodFlag = 0;
		HAL_Delay(100);
		openValve();
	}
	else
	{
		floodFlag = 1;
		HAL_Delay(100);
		closeValve();
	}
	HAL_Delay(500);
	valve_open = !floodFlag;
	alert();

	// Main loop
	while(1)
	{
		// Get current time
		uint32_t now;
		now = HAL_GetTick();

		// Check button hold duration
		if (now - holdTime >= 2000 && buttonState == 1)
		{
			buttonState = 0;        // Reset the button state
			// Check if the button is pressed and the valve is open
			if ((HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_RESET) && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET)
			{
				if(valve_open == 0)
				{
					openValve();            // Open the valve
				}
				strcpy(message, "valve open\r\n");
				console(message);
				floodFlag = 0;          	// Clear the flood flag
			}
			holdTime = 0;               	// Reset the hold time
		}

		// Close the valve if the flood flag is set
		if (floodFlag)
		{
			if(now - alert_time > 5000)
			{
				strcpy(message, "Flood\r\n");
				console(message);
				alert();
				alert_time = now;
			}
			if(valve_open == 1)
			{
				closeValve();
				strcpy(message, "valve closed\r\n");
				console(message);
			}
		}

		if(wupFlag == 1)
		{
			wupFlag = 0;
			statusled();
			monitorBattery();
		}
		/*
		// Measure battery voltage periodically
		if (now - last_batt_time > 60000)
		{
			monitorBattery();
			last_batt_time = now;                          			// Update last battery reading time
		}*/
		// Enter sleep mode after a specific period if no flood is detected
		if(now - sleep_time >= 10000 && !floodFlag)
		{
			strcpy(message, "Entering Sleep\r\n");
			console(message);
			wupFlag = 0;
			HAL_TIM_Base_Start_IT(&htim16);
			HAL_SuspendTick();
			//HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);   // Enable sleep mode
			HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON ,PWR_SLEEPENTRY_WFI);
		}
	}
}

// GPIO EXTI interrupt callback
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
	HAL_ResumeTick();
	sleep_time = HAL_GetTick();         // Update sleep time

	// Handle button press
	if(GPIO_Pin == GPIO_PIN_15)
	{
		buttonState++;      			// Increment button state
		holdTime = HAL_GetTick(); 		// Record button hold time
		if(buttonState>1)
		{
			buttonState = 0;
		}
		wupFlag = 1;
	}
	// Handle flood flag
	if(GPIO_Pin == GPIO_PIN_6)
	{
		floodFlag = 1; // Set flood flag
	}
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim == &htim16)
	{
		HAL_ResumeTick();
		wupFlag = 1;
	}
}

// Function to open the valve
void openValve()
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);    	// Activate valve
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);             	// Start PWM signal for valve control
	HAL_Delay(50);
	for(uint16_t i = 2100; i >= 900; i-=50)
	{
		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, i);   	// Set PWM duty cycle for valve opening
		HAL_Delay(30);
	}

	HAL_Delay(50);
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);              	// Stop PWM signal
	HAL_Delay(50);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);  	// Deactivate valve
	valve_open = 1;
}

// Function to close the valve
void closeValve()
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);    	// Activate valve
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);             	// Start PWM signal for valve control
	HAL_Delay(50);
	for(uint16_t i = 900; i <= 2100; i+=50)
	{
		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, i);   	// Set PWM duty cycle for valve Closing
		HAL_Delay(30);
	}
	HAL_Delay(50);
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);              	// Stop PWM signal
	HAL_Delay(50);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);  	// Deactivate valve
	valve_open = 0;
}

// Function to measure battery voltage
uint16_t measureBattery(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, SET);           	// Enable battery voltage measurement
	HAL_ADC_Start(&hadc1);                                	// Start ADC conversion
	HAL_ADC_PollForConversion(&hadc1, 1000);              	// Wait for ADC conversion to complete
	volatile uint16_t analogbatt = HAL_ADC_GetValue(&hadc1);       	// Read ADC value
	HAL_Delay(5);
	HAL_ADC_Stop(&hadc1);                                 	// Stop ADC conversion
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, RESET);         	// Disable battery voltage measurement

	// Check battery voltage threshold
	if(analogbatt < 2950)
	{
		Low_battery = 1;			// Set low battery flag if voltage is below threshold
	}
	else
	{
		Low_battery = 0;			// Reset low battery flag if voltage is above threshold
	}
	return analogbatt;  // Return battery voltage reading
}

void monitorBattery(void)
{
	uint16_t vBatt = measureBattery();            			// Measure battery voltage
	if(Low_battery)
	{
		batteryled();
	}
	sprintf(message, "Battery Voltage: %d\r\n", vBatt); 	// Format battery voltage message
	console(message);                             			// Send battery voltage message via UART
}
void statusled(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
}
// Function to activate battery LED
void batteryled(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);		// Activate battery LED
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
	HAL_Delay(200);											// Delay for LED indication
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);	// Deactivate battery LED
}

// Function to activate buzzer and warning LED
void alert(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);		// Activate buzzer
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);		// Activate warning LED
	HAL_Delay(1000);										// Delay for alert indication
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);	// Deactivate buzzer
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);	// Deactivate warning LED
}

// Function to transmit messages via UART
void console(char *log)
{
	HAL_UART_Transmit(&huart2, (uint8_t *)log, strlen(log), HAL_MAX_DELAY);  // Transmit message via UART
	HAL_Delay(10);
	memset(log, '\0', strlen(log));  // Clear message buffer
}
