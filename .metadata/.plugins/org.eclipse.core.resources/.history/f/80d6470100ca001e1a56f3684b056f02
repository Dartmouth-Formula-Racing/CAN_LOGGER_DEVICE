/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include "ds1307_for_stm32_hal.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define VERBOSE_DEBUGGING

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

I2C_HandleTypeDef hi2c1;

SD_HandleTypeDef hsd1;
DMA_HandleTypeDef hdma_sdmmc1_rx;
DMA_HandleTypeDef hdma_sdmmc1_tx;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_CAN1_Init(void);
static void MX_SDMMC1_SD_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef CAN_Filter_Config(void);

#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#define ENCODED_CAN_SIZE_BYTES 41
#define CAN_MESSAGES_TO_BUFFER 100
#define BUFFER_TOTAL_SIZE ENCODED_CAN_SIZE_BYTES*CAN_MESSAGES_TO_BUFFER
#define FILENAME_MAX_BYTES 256
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
FRESULT res;
DIR dir;		//Directory
FILINFO fno;	// File Info
const char *data_directory = "/CAN_DATA";

uint8_t POWER_STATE;
uint8_t NEW_LOG_FLAG;
char data_buffer[2][BUFFER_TOTAL_SIZE + 1]; // plus one for \00
uint8_t buffer_fill_level[2];
uint8_t current_buffer;
uint8_t is_buffer_filled = 0;

uint8_t curr_date;
uint8_t curr_month;
uint8_t curr_year;
uint8_t curr_hour;
uint8_t curr_minute;
uint8_t curr_second;
uint32_t starting_tick;

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CAN1_Init();
  MX_SDMMC1_SD_Init();
  MX_USART3_UART_Init();
  MX_FATFS_Init();
  MX_USB_DEVICE_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  DS1307_Init(&hi2c1);

	//States of our CAN DECODER
	typedef enum {
		TURN_ON,
		PERIPHERAL_INIT,
		CREATE_LOG_FILE,
		STANDBY,
		PERIPHERAL_ERROR,
		SWITCH_BUFFER,
		SD_CARD_WRITE,
		SD_CARD_WRITE_ERROR,
		USB_TRANSMIT,
		USB_TRANSMIT_ERROR,
		RESET_BUFFER,
		RESET_STATE,
		POWER_OFF
	} state_t;

	//Starting state is PERIPHERAL_INIT
	state_t state = TURN_ON;
	uint32_t byteswritten; /* File write/read counts */
#ifdef VERBOSE_DEBUGGING
	uint32_t buffer_emptyings = 0;
	uint32_t total_size = 0;
#endif

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		switch (state) {

		/**
		 * State: TURN_ON
		 *
		 * Description: Initializes middleware functions for working with
		 * SD card. Decides if the device is on or off
		 *
		 *
		 * Transition in:
		 * 	Starting state
		 * 	From POWER_OFF if power switch is in the on position
		 * 	From RESET_BUFFER if power switch is in the on position (button was pressed)
		 *
		 * Transition out:
		 * 	If power switch is set to on position -> PERIFPHERAL_INIT
		 * 	else (power switch is set to off) -> POWER_OFF
		 */
		case TURN_ON:
			MX_DMA_Init();
			MX_SDMMC1_SD_Init();
			MX_FATFS_Init();
			POWER_STATE = HAL_GPIO_ReadPin(PowerSwitch_GPIO_Port, PowerSwitch_Pin);
			state = POWER_STATE ? PERIPHERAL_INIT : POWER_OFF;
			NEW_LOG_FLAG = 0;
			break;

		/**
		 * State: PERIPHERAL_INIT
		 *
		 * Description: Empties data buffers, initializes CAN, and mounts SD card
		 *
		 * Transition in:
		 * 	From TURN_ON when power switch is set to on position
		 *
		 * Transition out:
		 * 	Always -> CREATE_LOG_FILE
		 */
		case PERIPHERAL_INIT:
			// Reset both buffers
			data_buffer[0][0] = '\00';
			data_buffer[1][0] = '\00';
			buffer_fill_level[0] = 0;
			buffer_fill_level[1] = 0;
			current_buffer = 0;

			// Turn Red LED on (Green turns off)
#ifdef VERBOSE_DEBUGGING
			printf("Initializing Peripherals...\r\n");
#endif
			HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin, GPIO_PIN_RESET); //Red LED

			// Initializing CAN
			if (HAL_CAN_Start(&hcan1) != HAL_OK) {
#ifdef VERBOSE_DEBUGGING
				printf("CAN could not start.\r\n");
#endif
				Error_Handler();
			}
			if (CAN_Filter_Config() != HAL_OK) {
#ifdef VERBOSE_DEBUGGING
				printf("CAN filter failed to set.\r\n");
#endif
				Error_Handler();
			}
#ifdef VERBOSE_DEBUGGING
			printf("CAN initialization succeeded...\r\n");
#endif

			// Mount and Format SD Card
			if (f_mount(&SDFatFS, SDPath, 0) != FR_OK) {
#ifdef VERBOSE_DEBUGGING
				printf("Mounting failed!\r\n");
#endif
				Error_Handler();
			}
#ifdef VERBOSE_DEBUGGING
			printf("SD initialization succeeded...\r\n");
#endif

			state = CREATE_LOG_FILE;
			break;


		/**
		 * State: CREATE_LOG_FILE
		 *
		 * Description: Updates date/time info, names current log with current time, starts to receive CAN messages
		 *
		 * Transition in:
		 * 	From PERIPHERAL_INIT, always
		 *
		 * Transition out:
		 * 	Always -> STANDBY
		 */
		case CREATE_LOG_FILE:
#ifdef VERBOSE_DEBUGGING
			printf("Creating new log file...\r\n");
#endif

			// Update current date/time info
			curr_date = DS1307_GetDate();
			curr_month = DS1307_GetMonth();
			curr_year = DS1307_GetYear();
			curr_hour = DS1307_GetHour();
			curr_minute = DS1307_GetMinute();
			curr_second = DS1307_GetSecond();
			starting_tick = HAL_GetTick();
#ifdef VERBOSE_DEBUGGING
			printf("%02d/%02d/20%02d %02d:%02d:%02d\r\n",
					curr_month, curr_date, curr_year, curr_hour, curr_minute, curr_second);
#endif
			if (f_stat(data_directory, &fno) != FR_OK) {
				if (f_mkdir(data_directory) != FR_OK) {
#ifdef VERBOSE_DEBUGGING
					printf("Data directory not present and failed to create it.");
#endif
					Error_Handler();
				}
			}

			// Creating new filename
			TCHAR filename[FILENAME_MAX_BYTES];
			snprintf(filename, FILENAME_MAX_BYTES, "%s/%02d-%02d-20%02d_(%02dh-%02dm-%02ds).log",
					data_directory,
					curr_month, curr_date, curr_year,
					curr_hour, curr_minute, curr_second);

#ifdef VERBOSE_DEBUGGING
			printf("New log name: %s \r\n", filename);
#endif

			// Open file for writing (Create)
			if (f_open(&SDFile, filename, FA_CREATE_ALWAYS | FA_WRITE)
					!= FR_OK) {
#ifdef VERBOSE_DEBUGGING
				printf("Failed to create new log file: %s ...!\r\n", filename);
#endif
				Error_Handler();
			}
#ifdef VERBOSE_DEBUGGING
			printf("Successfully created new log file: %s ...\r\n", filename);
#endif

			// Starting CANRx interrupts
			if (HAL_CAN_ActivateNotification(&hcan1,
					CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
				/* Notification Error */
#ifdef VERBOSE_DEBUGGING
				printf("Failed to activate CAN\r\n");
#endif
				Error_Handler();
			}

			// Turn Green LED on (turns Red LED off)
#ifdef VERBOSE_DEBUGGING
			printf("Ready to receive messages!\r\n");
#endif
			HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin, GPIO_PIN_SET); // Successful LED

			state = STANDBY;
			break;

		/**
		 * State: STANDBY
		 *
		 * Description: Waits to either power off, create a new log, or empty a buffer to the SD card
		 *
		 * Transition in:
		 * 	From CREATE_LOG_FILE, always
		 *
		 * Transition out:
		 * 	If power switch is set to off position or the new file button is clicked -> RESET_STATE
		 * 	Else if the current buffer is filled -> SD_CARD_WRITE
		 * 	Else -> STANDBY
		 */
		case STANDBY:
			if (!POWER_STATE || NEW_LOG_FLAG) //Power switch is off or new log file
				state = RESET_STATE;
			else if (is_buffer_filled) //Buffer is filled
				state = SD_CARD_WRITE;
			break;

		case PERIPHERAL_ERROR:
			break;

		case SWITCH_BUFFER:
			break;

		/**
		 * State: SD_CARD_WRITE
		 *
		 * Description: Writes the contents of the buffer not currently being filled to the SD card
		 *
		 * Transition in:
		 *	From STANDBY when a buffer fills
		 *
		 * Transition out:
		 *	Always -> USB_TRANSMIT
		 */
		case SD_CARD_WRITE:
			if (f_write(&SDFile, data_buffer[!current_buffer], BUFFER_TOTAL_SIZE, (void*) &byteswritten) != FR_OK || byteswritten == 0) {
#ifdef VERBOSE_DEBUGGING
				printf("Writing Failed!\r\n");
#endif
				Error_Handler();
			}

			state = USB_TRANSMIT;
			break;

		case SD_CARD_WRITE_ERROR:
			break;

		/**
		 * State: USB_TRANSMIT
		 *
		 * Description: Write the filled buffer over USB
		 *
		 * Transition in:
		 *	From SD_CARD_WRITE, always
		 *
		 * Transition out:
		 *	Always -> RESET_BUFFER
		 */
		case USB_TRANSMIT:
			CDC_Transmit_FS(data_buffer[!current_buffer], BUFFER_TOTAL_SIZE);
			state = RESET_BUFFER;
			break;

		case USB_TRANSMIT_ERROR:
			break;

		/**
		 * State: RESET_BUFFER
		 *
		 * Description: Prints debugging information then empties the buffer that was written to the SD and USB
		 *
		 * Transition in:
		 *	From USB_TRANSMIT, always
		 *
		 * Transition out:
		 *	Always -> STANDBY
		 */
		case RESET_BUFFER:
			// bookkeeping information (just for debugging)
#ifdef VERBOSE_DEBUGGING
			buffer_emptyings++;
			total_size += byteswritten;
			printf("emptied buffer %d\r\n", !current_buffer);
			printf("buffers emptied: %ld\r\n", buffer_emptyings);
			printf("Wrote buffer sizeof: %ld\r\n", byteswritten);
#endif
			// Reset buffer that was just sent to SD and USB
			data_buffer[!current_buffer][0] = '\00';
			buffer_fill_level[!current_buffer] = 0;
			is_buffer_filled = 0;

			byteswritten = 0;

			state = STANDBY;
			break;

		/**
		 * State: RESET_STATE
		 *
		 * Description: Disable CAN interrupt, close the log file, and unmount the SD
		 *
		 * Transition in:
		 *	From STANDBY if the new file button was pressed or the switch was left in the power off position
		 *
		 * Transition out:
		 *	If power switch is in off position -> POWER_OFF
		 *	Else (button was pressed) -> TURN_ON
		 */
		case RESET_STATE:
			// Turn off CAN interrupt
			HAL_CAN_DeactivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
			HAL_CAN_Stop(&hcan1);

			// Turn Red LED on (Green LED turns off)
			HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin, GPIO_PIN_RESET); //Red LED

#ifdef VERBOSE_DEBUGGING
			// Debugging information
			printf("total sizeof: %ld\r\n", total_size);
			total_size = 0;
			buffer_emptyings = 0;
			printf("Unmounting SD Card!\r\n");
#endif
			f_close(&SDFile);
			f_mount(0, (TCHAR const*) NULL, 0);

			if (!POWER_STATE) {
				state = POWER_OFF;
#ifdef VERBOSE_DEBUGGING
				printf("Turning off!\r\n");
#endif
			}
			else {
				state = TURN_ON; // button was pressed
			}
			break;

		/**
		 * State: POWER_OFF
		 *
		 * Description: Stay in this state until the power switch is put in the on position
		 *
		 * Transition in:
		 *	From RESET_STATE, always
		 *
		 * Transition out:
		 *	If power switch is in the on position -> TURN_ON
		 *	Else -> POWER_OFF
		 */
		case POWER_OFF:
			if (POWER_STATE) {
				state = TURN_ON;

#ifdef VERBOSE_DEBUGGING
				printf("\r\nTurning back on!\r\n");
#endif
			}
			break;

		default:
			HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin,
								GPIO_PIN_RESET); // Red LED

#ifdef VERBOSE_DEBUGGING
			printf("CAN logger in unknown state!\r\n");
#endif
			break;

		}
	}
	return 0;

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 3;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00506682;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SDMMC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDMMC1_SD_Init(void)
{

  /* USER CODE BEGIN SDMMC1_Init 0 */

  /* USER CODE END SDMMC1_Init 0 */

  /* USER CODE BEGIN SDMMC1_Init 1 */

  /* USER CODE END SDMMC1_Init 1 */
  hsd1.Instance = SDMMC1;
  hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsd1.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
  hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_ENABLE;
  hsd1.Init.ClockDiv = 0;
  /* USER CODE BEGIN SDMMC1_Init 2 */
	if (HAL_SD_Init(&hsd1) != HAL_OK) {
		Error_Handler();
	}
  /* USER CODE END SDMMC1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  /* DMA2_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : Error_LED_Pin */
  GPIO_InitStruct.Pin = Error_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Error_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NewLogBtn_Pin */
  GPIO_InitStruct.Pin = NewLogBtn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(NewLogBtn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PE11 */
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF6_DFSDM1;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PowerSwitch_Pin */
  GPIO_InitStruct.Pin = PowerSwitch_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PowerSwitch_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SD_CD_Pin USB_OverCurrent_Pin */
  GPIO_InitStruct.Pin = SD_CD_Pin|USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void Get_and_Append_CAN_Message_to_Buffer() {
	CAN_RxHeaderTypeDef RxHeader;
	uint8_t rcvd_msg[8];

	if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, rcvd_msg)
			!= HAL_OK){
#ifdef VERBOSE_DEBUGGING
		printf("Failed to get CAN message\r\n");
#endif
		Error_Handler();
	}

	uint16_t data1 = (rcvd_msg[0] << 8) + rcvd_msg[1];
	uint16_t data2 = (rcvd_msg[2] << 8) + rcvd_msg[3];
	uint16_t data3 = (rcvd_msg[4] << 8) + rcvd_msg[5];
	uint16_t data4 = (rcvd_msg[6] << 8) + rcvd_msg[7];

	char encodedData[ENCODED_CAN_SIZE_BYTES];

	// consider writing raw bytes
	snprintf(encodedData, ENCODED_CAN_SIZE_BYTES + 1,
			"(%010ld) X %08lX#%04X%04X%04X%04X\n", HAL_GetTick(),
			RxHeader.ExtId, data1, data2, data3, data4);

	strcat(current_buffer ? data_buffer[1] : data_buffer[0], encodedData);
	buffer_fill_level[current_buffer]++;
}

HAL_StatusTypeDef CAN_Filter_Config(void) {
	CAN_FilterTypeDef filter;

	uint32_t filter_mask = 0x00000000;
	uint32_t filter_id = 0x000A0000;

	filter.FilterIdHigh = ((filter_id << 5) | (filter_id >> (32 - 5))) & 0xFFFF; // STID[10:0] & EXTID[17:13]
	filter.FilterIdLow = (filter_id >> (11 - 3)) & 0xFFF8; // EXID[12:5] & 3 Reserved bits
	filter.FilterMaskIdHigh = ((filter_mask << 5) | (filter_mask >> (32 - 5)))
			& 0xFFFF;
	filter.FilterMaskIdLow = (filter_mask >> (11 - 3)) & 0xFFF8;

	filter.FilterFIFOAssignment = CAN_RX_FIFO0;
	filter.FilterBank = 0;
	filter.FilterMode = CAN_FILTERMODE_IDMASK;
	filter.FilterScale = CAN_FILTERSCALE_32BIT;
	filter.FilterActivation = ENABLE;

	return HAL_CAN_ConfigFilter(&hcan1, &filter);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
	if (buffer_fill_level[0] == CAN_MESSAGES_TO_BUFFER
			&& buffer_fill_level[1] == CAN_MESSAGES_TO_BUFFER)
	{
#ifdef VERBOSE_DEBUGGING
		printf("Buffers are full\r\n");
#endif
		Error_Handler();
	}

	Get_and_Append_CAN_Message_to_Buffer();

	if (buffer_fill_level[current_buffer] == CAN_MESSAGES_TO_BUFFER) {
		is_buffer_filled = 1;
		current_buffer = !current_buffer;
	}
}

/**
 * @brief  Retargets the C library printf function to the USART.
 * @param  None
 * @retval None
 */
PUTCHAR_PROTOTYPE {
	/* Place your implementation of fputc here */
	/* e.g. write a character to the USART1 and Loop until the end of transmission */
	HAL_UART_Transmit(&huart3, (uint8_t*) &ch, 1, 0xFFFF);

	return ch;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
#ifdef VERBOSE_DEBUGGING
	printf("\r\nError Handler Reached\r\n");
#endif
	HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin, GPIO_PIN_RESET);

	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
