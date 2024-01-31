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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

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
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef CAN_Filter_Config(void);

#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#define ENCODED_CAN_SIZE_BYTES 41
#define CAN_MESSAGES_TO_BUFFER 128
#define FILENAME_MAX_BYTES 256
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
FRESULT res;
DIR dir;		//Directory
FILINFO fno;	// File Info

int timestamp = 0;
int received = 0;
char encodedData[ENCODED_CAN_SIZE_BYTES];
int messageReceived = 0;
uint32_t byteswritten; /* File write/read counts */
CAN_RxHeaderTypeDef RxHeader;
uint8_t rcvd_msg[8];
uint8_t rtext[_MAX_SS];/* File read buffer */
uint8_t POWER_STATE;

char buffer1[ENCODED_CAN_SIZE_BYTES * CAN_MESSAGES_TO_BUFFER + 1];
char buffer2[ENCODED_CAN_SIZE_BYTES * CAN_MESSAGES_TO_BUFFER + 1];
uint8_t double_buffer_fill_level[2];
uint8_t filling_buffer;
uint32_t buffer_emptyings = 0;
uint8_t buffer_filled = 0;
uint32_t total_size = 0;
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
  /* USER CODE BEGIN 2 */

	//States of our CAN DECODER
	typedef enum {
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
	POWER_STATE = HAL_GPIO_ReadPin(PowerSwitch_GPIO_Port, PowerSwitch_Pin);
	state_t state = POWER_STATE ? PERIPHERAL_INIT : POWER_OFF;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		switch (state) {
		case PERIPHERAL_INIT:
			buffer1[0] = '\00';
			buffer2[0] = '\00';
			double_buffer_fill_level[0] = 0;
			double_buffer_fill_level[1] = 0;
			filling_buffer = 0;

			printf("Initializing Peripherals...\r\n");
			HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin,
					GPIO_PIN_RESET); //Red LED

			//Initializing CAN
			if (HAL_CAN_Start(&hcan1) != HAL_OK)
				Error_Handler();
			else if (CAN_Filter_Config() != HAL_OK)
				Error_Handler();

			printf("CAN initialization succeeded...\r\n");

			//Mount and Format SD Card
			if (f_mount(&SDFatFS, SDPath, 0) != FR_OK) {
				printf("Mounting failed!\r\n");
				Error_Handler();
			}

			printf("SD initialization succeeded...\r\n");

			state = CREATE_LOG_FILE;

			break;
		case CREATE_LOG_FILE:
			printf("Creating new log file...\r\n");

			TCHAR filename[FILENAME_MAX_BYTES];

			if (f_opendir(&dir, "/CAN_DATA") != FR_OK) {
				printf("Failed to open root directory!\r\n");
				Error_Handler();
			}
			char last_file_number[5];
			uint16_t max_file_number = 0;
			do {
				f_readdir(&dir, &fno);
				if (fno.fname[0] != 0){
					for(int i=4; i<9; i++)
						last_file_number[i-4] = fno.fname[i];

					if (max_file_number < strtol(last_file_number, NULL, 10))
						max_file_number = strtol(last_file_number, NULL, 10);

					printf("File found: %s\n\r", fno.fname); // Print File Name
				}
			} while (fno.fname[0] != 0);

			snprintf(filename, FILENAME_MAX_BYTES, "/CAN_DATA/CAN_%05d.log", max_file_number + 1);

			f_closedir(&dir);
			//Open file for writing (Create)
			if (f_open(&SDFile, filename, FA_CREATE_ALWAYS | FA_WRITE)
					!= FR_OK) {
				printf("Failed to create new log file: %s ...!\r\n", filename);
				Error_Handler();
			}
			printf("Starting new log file: %s ...\r\n", filename);

			if (HAL_CAN_ActivateNotification(&hcan1,
					CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
				/* Notification Error */
				Error_Handler();
			}

			HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin, GPIO_PIN_SET); // Green LED

			state = STANDBY;
			break;

		case STANDBY:
			if (POWER_STATE & buffer_filled)
				state = SD_CARD_WRITE;
			else if (!POWER_STATE)
				state = RESET_STATE;
			break;

		case PERIPHERAL_ERROR:
			break;

		case SWITCH_BUFFER:
			break;

		case SD_CARD_WRITE:
			res = f_write(&SDFile, filling_buffer ? buffer1 : buffer2,
			ENCODED_CAN_SIZE_BYTES * CAN_MESSAGES_TO_BUFFER,
					(void*) &byteswritten);
			if ((byteswritten == 0) || (res != FR_OK)) {
				printf("\r\nWriting Failed!\r\n");
				Error_Handler();
			}

			state = USB_TRANSMIT;
			break;

		case SD_CARD_WRITE_ERROR:
			break;

		case USB_TRANSMIT:
			CDC_Transmit_FS(filling_buffer ? buffer1 : buffer2,
			ENCODED_CAN_SIZE_BYTES * CAN_MESSAGES_TO_BUFFER);

			state = RESET_BUFFER;
			break;

		case USB_TRANSMIT_ERROR:
			break;

		case RESET_BUFFER:
			buffer_emptyings++;
			printf("emptied buffer %d\n\r", !filling_buffer);
			printf("buffers emptied: %ld\n\r", buffer_emptyings);
			printf("sizeof: %ld\n\r", byteswritten);

			total_size += byteswritten;
			if (filling_buffer) {
				buffer1[0] = '\00';
			} else {
				buffer2[0] = '\00';
			}
			double_buffer_fill_level[!filling_buffer] = 0;
			buffer_filled = 0;

			state = STANDBY;
			break;

		case RESET_STATE:
			HAL_CAN_Stop(&hcan1);
			HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin,
					GPIO_PIN_RESET); //Red LED

			printf("total sizeof: %ld\n\r", total_size);

			printf("\r\nUnmounting!\r\n");
			f_close(&SDFile);
			f_mount(0, (TCHAR const*) NULL, 0);

			printf("Turning off!\n\r");
			state = POWER_OFF;
			break;

		case POWER_OFF:
			HAL_Delay(1000);

			if (POWER_STATE) {
				printf("Turning back on!\n\r");
				HAL_NVIC_SystemReset();
			}
			break;

		default:
			printf("CAN logger in unknown state!");
			HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin,
					GPIO_PIN_RESET); // Red LED
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
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void Get_and_Append_CAN_Message_to_Buffer() {
	if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, rcvd_msg)
			!= HAL_OK)
		Error_Handler();

	uint16_t data1 = (rcvd_msg[0] << 8) + rcvd_msg[1];
	uint16_t data2 = (rcvd_msg[2] << 8) + rcvd_msg[3];
	uint16_t data3 = (rcvd_msg[4] << 8) + rcvd_msg[5];
	uint16_t data4 = (rcvd_msg[6] << 8) + rcvd_msg[7];

	snprintf(encodedData, ENCODED_CAN_SIZE_BYTES + 1,
			"(%010ld) X %08lX#%04X%04X%04X%04X\n", HAL_GetTick(),
			RxHeader.ExtId, data1, data2, data3, data4);

	strcat(filling_buffer ? buffer2 : buffer1, encodedData);
	double_buffer_fill_level[filling_buffer]++;
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
	if (double_buffer_fill_level[0] == CAN_MESSAGES_TO_BUFFER
			&& double_buffer_fill_level[1] == CAN_MESSAGES_TO_BUFFER)
		Error_Handler();

	Get_and_Append_CAN_Message_to_Buffer();

	if (double_buffer_fill_level[filling_buffer] == CAN_MESSAGES_TO_BUFFER) {
		buffer_filled = 1;
		filling_buffer = !filling_buffer;
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
	printf("\r\nError Handler Reached\r\n");
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
