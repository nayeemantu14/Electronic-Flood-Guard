// This program controls a flood guard system using an STM32 microcontroller.

// Including necessary libraries
#include "main.h"          					// Main header file
#include <stdio.h>         					// Standard input/output library
#include <string.h>        					// String manipulation library
#include <stdbool.h>
// External peripheral handlers declaration
extern ADC_HandleTypeDef hadc1;      		// ADC handler
extern TIM_HandleTypeDef htim3;      		// Timer 3 handler
extern TIM_HandleTypeDef htim16;     		// Timer 16 handler
extern UART_HandleTypeDef huart2;    		// UART handler

// Global variable declaration
char message[40];                     		// Buffer to store messages
uint8_t wupFlag = 0;  				  		// Wake-up flag
volatile static uint8_t valve_open = 0;
volatile static uint8_t floodFlag = 0;    	// Flag to indicate flooding
volatile static uint8_t buttonState = 0;  	// State of the button (volatile due to ISR modification)
volatile static uint32_t holdTime = 0;    	// Duration the button is held (volatile due to ISR modification)
uint32_t last_batt_time = 0;              	// Last time of battery reading
uint32_t sleep_time = 0; 					// Time to enter sleep mode
uint32_t alert_time = 0;

// Function prototypes
void openValve();                    		// Function to open the valve
void closeValve();                    		// Function to close the valve
void alert(void);							// Function to implement Buzzer and Warning LED
uint16_t measureBattery(void);        		// Function to measure battery voltage
void console(char *log);              		// Function to transmit messages via UART

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
    }
    else
    {
        floodFlag = 1;
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
        if (now - holdTime >= 1500 && buttonState == 1)
        {
        	// Check if the button is pressed and the valve is open
        	if ((HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET)
        	{
        		openValve();            // Open the valve
        		strcpy(message, "valve open\r\n");
        		console(message);
        		floodFlag = 0;          // Clear the flood flag
        		buttonState = 0;        // Reset the button state
        	}
        	holdTime = 0;               // Reset the hold time
        }
        else if(buttonState > 1)
        {
        	buttonState = 0;            // Reset button state if it exceeds 2
        }

        // Close the valve if the flood flag is set
        if (floodFlag)
        {
        	if(valve_open)
        	{
        		for(int i=0; i<=1;i++)
        		{
        			closeValve();
        			HAL_Delay(5);
        		}

        		strcpy(message, "valve closed\r\n");
        		console(message);
        		alert();
        	}
        	else
        	{
        		if(now - alert_time > 5000)
        		{
                	strcpy(message, "Flood\r\n");
                	console(message);
        			alert();
        			alert_time = now;
        		}
        	}
        }

        // Measure battery voltage periodically
        if (now - last_batt_time > 5000) {
            uint16_t vBatt = measureBattery();            			// Measure battery voltage
            sprintf(message, "Battery Voltage = %d\r\n", vBatt); 	// Format battery voltage message
            console(message);                             			// Send battery voltage message via UART
            last_batt_time = now;                          			// Update last battery reading time
        }
        // Enter sleep mode after a specific period if no flood is detected
        if(now - sleep_time >= 15000 && !floodFlag)
        {
        	strcpy(message, "Entering Sleep\r\n");
        	console(message);
        	HAL_PWR_EnableSleepOnExit();   // Enable sleep mode
        }
    }
}

// GPIO EXTI interrupt callback
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    HAL_PWR_DisableSleepOnExit();      	// Disable sleep mode
    sleep_time = HAL_GetTick();         // Update sleep time

    // Handle button press
    if(GPIO_Pin == GPIO_PIN_13)
    {
        buttonState++;      			// Increment button state
        holdTime = HAL_GetTick(); 		// Record button hold time
    }
    // Handle flood flag
    if(GPIO_Pin == GPIO_PIN_6)
    {
        floodFlag = 1; // Set flood flag
    }
}

// Function to open the valve
void openValve()
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);    	// Activate valve
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);             	// Start PWM signal for valve control
    HAL_Delay(5);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 1100);   	// Set PWM duty cycle for valve opening
    HAL_Delay(5);
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);              	// Stop PWM signal
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);  	// Deactivate valve
    valve_open = 1;
}

// Function to close the valve
void closeValve()
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);    	// Activate valve
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);             	// Start PWM signal for valve control
    HAL_Delay(5);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 2050);   	// Set PWM duty cycle for valve closing
    HAL_Delay(5);
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);              	// Stop PWM signal
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);  	// Deactivate valve
    valve_open = 0;
}

// Function to measure battery voltage
uint16_t measureBattery(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, SET);           	// Enable battery voltage measurement
    HAL_ADC_Start(&hadc1);                                	// Start ADC conversion
    HAL_ADC_PollForConversion(&hadc1, 1000);              	// Wait for ADC conversion to complete
    uint16_t analogbatt = HAL_ADC_GetValue(&hadc1);       	// Read ADC value
    HAL_Delay(5);
    HAL_ADC_Stop(&hadc1);                                 	// Stop ADC conversion
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, RESET);         	// Disable battery voltage measurement

    // Check battery voltage threshold
    if(analogbatt < 2500)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, SET);        	// Trigger alert if battery voltage is below threshold
        HAL_Delay(200);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, RESET);
    }
    return analogbatt;  // Return battery voltage reading
}

void alert(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
	HAL_Delay(1000);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
}

// Function to transmit messages via UART
void console(char *log)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)log, strlen(log), HAL_MAX_DELAY);  // Transmit message via UART
    HAL_Delay(10);
    memset(log, '\0', strlen(log));  // Clear message buffer
}
