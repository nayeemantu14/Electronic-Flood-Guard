// Program Description: This program controls a flood guard system using an STM32 microcontroller.

// Including necessary libraries
#include "main.h"          					// Include main header file
#include <stdio.h>         					// Include standard input/output library
#include <string.h>        					// Include string manipulation library
#include <stdbool.h>						// Include boolean data type

// External peripheral handlers declaration
extern ADC_HandleTypeDef hadc1;      		// Declare ADC handler
extern TIM_HandleTypeDef htim3;      		// Declare Timer 3 handler
extern UART_HandleTypeDef huart2;    		// Declare UART handler
extern RTC_HandleTypeDef hrtc;
extern TIM_HandleTypeDef htim16;

// Global variable declaration
char message[40];                     		// Buffer to store messages

uint8_t wupFlag = 1;  						// Initialize wake-up flag
volatile static uint8_t mbatt_counter;
static uint8_t Low_battery;					// Initialize low battery flag

volatile static uint8_t valve_open;			// Initialize valve open flag
volatile static uint8_t floodFlag = 0;    	// Initialize flood flag
volatile static uint8_t buttonState = 0;  	// Initialize button state
volatile static uint32_t holdTime = 0;    	// Initialize button hold time
volatile static uint32_t releaseTime = 0;	// Initialize button release time
volatile static uint32_t pressDuration = 0; // Initialize button press duration

static uint32_t alert_time = 0;					// Initialize alert time
static uint32_t sleep_time = 0;

// Function prototypes
void openValve();                    		// Function prototype for opening the valve
void closeValve();                    		// Function prototype for closing the valve
void alert(void);							// Function prototype for activating the buzzer and warning LED
void resetFloodEvent();						// Function prototype for resetting the flood event
uint16_t measureBattery(void);        		// Function prototype for measuring battery voltage
void monitorBattery(void);					// Function prototype for monitoring battery voltage
void statusled(void);						// Function prototype for system status led
void batteryled(void);						// Function prototype for activating battery LED
void console(char *log);              		// Function prototype for transmitting messages via UART

// Main application function
int app_main()
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
	alert();
	// Main loop
	while(1)
	{
		// Get current time
		uint32_t now;
		now = HAL_GetTick();
		// Test Mode activated by long pressing the button
		if(pressDuration >= 2000 && !floodFlag)
		{
			pressDuration = 0;
			statusled();
			closeValve();
			alert();
			HAL_Delay(500);
			statusled();
			openValve();
		}
		// Servicing the short button press during a flood event
		else if(pressDuration <= 300 && pressDuration >= 50)
		{
			pressDuration = 0;
			resetFloodEvent();
		}
		// Close the valve if the flood flag is set
		if (floodFlag)
		{
			if(now - alert_time > 5000)
			{
				alert_time = now;
				strcpy(message, "Flood\r\n");
				console(message);
				alert();
			}
			if(valve_open == 1)
			{
				closeValve();
				strcpy(message, "valve closed\r\n");
				console(message);
			}
		}

		if(now - sleep_time >= 5000 && !floodFlag && wupFlag)
		{
			statusled();
			if (mbatt_counter == 59)
			{
				monitorBattery();
			}
			wupFlag = 0;
			HAL_SuspendTick();
			HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);    	// Enable Stop mode
		}
	}
	return 0;
}

// Callback function for rising edge interrupt on GPIO EXTI line
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
	HAL_ResumeTick();
	sleep_time = HAL_GetTick();
	wupFlag = 1;
	if(GPIO_Pin == GPIO_PIN_15)
	{
		if (buttonState == 1)
		{
			releaseTime = HAL_GetTick();
			pressDuration = releaseTime - holdTime;
		}
		buttonState = 0;
	}
}

// Callback function for falling edge interrupt on GPIO EXTI line
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
	HAL_ResumeTick();
	sleep_time = HAL_GetTick();
	wupFlag = 1;
	mbatt_counter = 59;

	// Handle button press
	if(GPIO_Pin == GPIO_PIN_15)
	{
		buttonState = 1;
		holdTime = HAL_GetTick(); 		// Record button hold time
	}
	// Handle flood flag
	if(GPIO_Pin == GPIO_PIN_6)
	{
		HAL_TIM_Base_Start_IT(&htim16);
	}
}

// Callback function for TIM16 period elapsed interrupt
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
	HAL_ResumeTick();
	sleep_time = HAL_GetTick();
	wupFlag = 1;
	mbatt_counter++;
	if(mbatt_counter > 59)
	{
		mbatt_counter = 0;
	}
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* Prevent unused argument(s) compilation warning */
  if(htim == &htim16)
  {
	  if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_RESET)
	  {
		  floodFlag = 1; // Set flood flag
	  }
	  HAL_TIM_Base_Stop_IT(&htim16);
  }
}
// Function to open the valve
void openValve()
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);    	// Activate valve
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);             	// Start PWM signal for valve control
	HAL_Delay(50);
	for(uint16_t i = 1800; i >= 900; i-=50)
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
	for(uint16_t i = 900; i <= 1800; i+=50)
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

// Function to reset flood event
void resetFloodEvent()
{
	// Check if the button is pressed and the valve is open
	if ((HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_SET) && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET)
	{
		if(valve_open == 0)
		{
			openValve();            // Open the valve
		}
		strcpy(message, "valve open\r\n");
		console(message);
		floodFlag = 0;          	// Clear the flood flag
	}
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

// Function to monitor battery voltage
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

// Function to control status LED
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
