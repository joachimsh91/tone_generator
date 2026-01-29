/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "i2c-lcd.h"
#include <stdlib.h>
#include <ctype.h>
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
DAC_HandleTypeDef hdac;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
void myprintf(const char *fmt, ...);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Printf over UART
void myprintf(const char *fmt, ...) {
  static char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  int len = strlen(buffer);
  HAL_UART_Transmit(&huart2, (uint8_t*)buffer, len, -1);

}

#ifdef __GNUC__
int __io_putchar(int ch)
#else
int fputc(int ch, FILE *f)
#endif
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}



#include <math.h>

FATFS FatFs;
#define NEXT_BUTTON_GPIO_PORT GPIOA
#define NEXT_BUTTON_PIN GPIO_PIN_9
#define PLAY_BUTTON_GPIO_PORT GPIOA
#define PLAY_BUTTON_PIN GPIO_PIN_8

#define SAMPLE_RATE 22050.0f
#define MAX_FILES 10
char Files[MAX_FILES][64];

static FIL audioFile;
static volatile uint32_t fileIndex = 0;
uint8_t Count = 0;

volatile float phase = 0.0f;
volatile float phaseStep = 0.0f;
volatile float volume = 0.2f;

static volatile bool isPlaying = false;

typedef enum {
	STATE_SELECT,
	STATE_PLAYING
} AppState;

typedef enum {
    WAVE_SINE = 0,
    WAVE_TRIANGLE,
    WAVE_SAW,
    WAVE_SQUARE
} WaveType;

typedef struct {
    WaveType wave;
    float frequency;
    float amplitude;
    uint32_t duration_ms;
} Preset;

Preset currentPreset;

extern DAC_HandleTypeDef hdac;
extern TIM_HandleTypeDef htim6;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
	// Timer interrupt callback that runs every time TIM6 generates a period-interrupt
{
    if (htim->Instance == TIM6 && isPlaying) {

        float sample = 0.0f;

        switch (currentPreset.wave) {

            case WAVE_SINE:
                sample = sinf(phase);
                break;

            case WAVE_SQUARE:
                sample = (sinf(phase) >= 0) ? 1.0f : -1.0f;
                break;

            case WAVE_TRIANGLE:
                sample = (2.0f / 3.14159265f) * asinf(sinf(phase));
                break;

            case WAVE_SAW:
                sample = (2.0f / 3.14159265f) * (phase - 3.14159265f);
                break;
        }

        // Scales with amplitude (0–1)
        sample *= currentPreset.amplitude * volume;

        // Converts from (-1)-(1) to 0-4095 (12-bit DAC)
        uint16_t dacValue = (uint16_t)((sample + 1.0f) * 2047.5f);

        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dacValue);

        // Updates phase
        phase += phaseStep;
        if (phase >= 2.0f * 3.14159265f)
            phase -= 2.0f * 3.14159265f;
    }
}


bool next_button_pressed(void) {
  return HAL_GPIO_ReadPin(NEXT_BUTTON_GPIO_PORT, NEXT_BUTTON_PIN) == GPIO_PIN_RESET;
}

bool play_button_pressed(void) {
return HAL_GPIO_ReadPin(PLAY_BUTTON_GPIO_PORT, PLAY_BUTTON_PIN) == GPIO_PIN_RESET;
}

void show_current_file(void) {
  // Shows current preset selected
  lcd_clear();
  lcd_put_cur(0, 0);
  lcd_send_string("Tone:");

  // Copies and fixes filename
  char name[64];
  strncpy(name, Files[fileIndex], sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';

  // Removes ".txt"
  char *dot = strrchr(name, '.');
  if (dot) {
	  *dot = '\0';
  }

  int len = strlen(name);

  if (len <= 16) {
	  // Fits on a single line
	  lcd_put_cur(1, 0);
	  lcd_send_string(name);
  } else {
	  // Scrolls text over the line
	  char scrollBuffer[17]; // 16 signs + null-termination
	  for (int i = 0; i <= len - 16; i++) {
		  strncpy(scrollBuffer, name + i, 16);
		  scrollBuffer[16] = '\0'; // null-terminates

		  lcd_put_cur(1, 0);
		  lcd_send_string("                "); // Clears line
		  lcd_put_cur(1, 0);
		  lcd_send_string(scrollBuffer);

		  HAL_Delay(100);
	  }

	  // Shows last square a little bit longer
	  HAL_Delay(300);
  }
}

void show_current_file_playing(void) {
  // Shows current preset playing
  lcd_clear();
  lcd_put_cur(0, 0);
  lcd_send_string("Now playing:");

  // Copies and fixes filename
  char name[64];
  strncpy(name, Files[fileIndex], sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';

  // Removes ".txt"
  char *dot = strrchr(name, '.');
  if (dot) {
	  *dot = '\0';
  }

  int len = strlen(name);

  if (len <= 16) {
	  // Fits one line
	  lcd_put_cur(1, 0);
	  lcd_send_string(name);
  } else {
	  // Scrolls text
	  char scrollBuffer[17]; // 16 signs + null-termination
	  for (int i = 0; i <= len - 16; i++) {
		  strncpy(scrollBuffer, name + i, 16);
		  scrollBuffer[16] = '\0'; // null-terminates

		  lcd_put_cur(1, 0);
		  lcd_send_string("                "); // Clears line
		  lcd_put_cur(1, 0);
		  lcd_send_string(scrollBuffer);

		  HAL_Delay(100);
	  }

	  // Shows last square a little bit longer
	  HAL_Delay(300);
  }
}


void read_files(void) {
	// Reads preset filenames stored on SD card
    DIR dir;
    FRESULT res;
    FILINFO fno;

    res = f_opendir(&dir, "/");  // root folder
    if (res != FR_OK) {
        lcd_send_string("Open fail");
        return;
    }

    while ((f_readdir(&dir, &fno) == FR_OK) && fno.fname[0] && Count < MAX_FILES) {
        // Reads filenames only, not folders
        if (!(fno.fattrib & AM_DIR)) {
            // Check if filename end with ".txt" (not case sensitive)
            char *ext = strrchr((char*)fno.fname, '.');
            // Stores filename to list
            if (ext && (strcasecmp(ext, ".txt") == 0)) {
                strncpy(Files[Count], (char*)fno.fname, 63);
                Files[Count][63] = '\0';
                Count++;
            }
        }
    }

    f_closedir(&dir);
}

bool parse_preset_file(const char *filename, Preset *preset) {
	// Parses info from preset files and stores to preset-structure
    FIL file;
    char line[64];

    if (f_open(&file, filename, FA_READ) != FR_OK) {
        lcd_send_string("File open fail");
        return false;
    }

    // Sets default values
    preset->wave = WAVE_SINE;
    preset->frequency = 440.0f;
    preset->amplitude = 0.8f;
    preset->duration_ms = 0;

    while (f_gets(line, sizeof(line), &file)) {
        // Removes newline
        line[strcspn(line, "\r\n")] = 0;

        // Waveform
        if (strncasecmp(line, "WAVE=", 5) == 0) {
            char *val = &line[5];
            if (strcasecmp(val, "SINE") == 0) preset->wave = WAVE_SINE;
            else if (strcasecmp(val, "TRIANGLE") == 0) preset->wave = WAVE_TRIANGLE;
            else if (strcasecmp(val, "SAW") == 0) preset->wave = WAVE_SAW;
            else if (strcasecmp(val, "SQUARE") == 0) preset->wave = WAVE_SQUARE;
        }

        // Frequency
        else if (strncasecmp(line, "FREQ=", 5) == 0) {
            preset->frequency = atof(&line[5]);
        }

        // Amplitude
        else if (strncasecmp(line, "AMP=", 4) == 0) {
            preset->amplitude = atof(&line[4]);
        }

        // Duration
        else if (strncasecmp(line, "DUR=", 4) == 0) {
            preset->duration_ms = (uint32_t)atoi(&line[4]);
        }
    }

    f_close(&file);
    return true;
}

void print_preset(Preset *preset) {
	// Prints preset info to console
    char *waveName;

    switch (preset->wave) {
        case WAVE_SINE:     waveName = "SINE"; break;
        case WAVE_TRIANGLE: waveName = "TRIANGLE"; break;
        case WAVE_SAW:      waveName = "SAW"; break;
        case WAVE_SQUARE:   waveName = "SQUARE"; break;
        default:            waveName = "UNKNOWN"; break;
    }

    printf("Preset info:\r\n");
    printf("Wave: %s\r\n", waveName);
    printf("Freq: %.2f Hz\r\n", preset->frequency);
    printf("Amp: %.2f\r\n", preset->amplitude);
    printf("Duration: %lu ms\r\n", preset->duration_ms);
}



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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_FATFS_Init();
  MX_I2C1_Init();
  MX_DAC_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */

  	// Variables for button logic (buttons previously pressed)
	bool nextPressedPrev;
	bool playPressedPrev;

	// Initializes LCD screen
	lcd_init();
	HAL_Delay(500);
	lcd_clear();

	// Mounts SD card
	f_mount(&FatFs, "", 1);
	if (f_mount(&FatFs, "", 1) != FR_OK) {
	    lcd_send_string("Mount failed");
	    while (1);
	}
	// Reads filenames on SD card
	read_files();

	playPressedPrev = play_button_pressed();
	nextPressedPrev = next_button_pressed();
	AppState state = STATE_SELECT;
	if (Count > 0) {
	show_current_file();
	} else {
	lcd_send_string("No .txt found");
	}


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
	  bool nextNow = next_button_pressed();
	  bool playNow = play_button_pressed();

	  switch (state) {
	    case STATE_SELECT:
	      // Prints next preset filename to LCD when select button is pressed
	      if (nextNow && !nextPressedPrev) {
	        fileIndex = (fileIndex + 1) % Count;
	        show_current_file();
	      }

          if (playNow && !playPressedPrev) {
        	  // Plays selected tone when play button is pressed

              // Parses chosen preset
              if (parse_preset_file(Files[fileIndex], &currentPreset)) {
                  // Prints preset info to console
                  print_preset(&currentPreset);

                  // Initializes tone parameters before playing starts
                  phase = 0;
                  phaseStep = (2.0f * 3.14159265f * currentPreset.frequency) / SAMPLE_RATE;
                  // Timer and DAC starts
                  HAL_TIM_Base_Start_IT(&htim6);
                  HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
                  isPlaying = true;
                  show_current_file_playing();
                  state = STATE_PLAYING;
              } else {
                  lcd_send_string("Preset fail");
              }
          }
          break;

	    case STATE_PLAYING:
	      // Stops playing tone if play button is pressed again
	      if (playNow && !playPressedPrev) {
	    	HAL_TIM_Base_Stop_IT(&htim6);
	    	HAL_DAC_Stop(&hdac, DAC_CHANNEL_1);
	        isPlaying = false;
	        f_close(&audioFile);
	        show_current_file();
	        state = STATE_SELECT;
	      }
	      break;
	  }
	  // Updates button variables
	  nextPressedPrev = nextNow;
	  playPressedPrev = playNow;

	  HAL_Delay(20);         // debounce
	}

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 64;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

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
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;  // Juster etter klokkehastighet
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 1452;     // Juster for ønsket samplefrekvens
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  // ** Viktig: Aktiver interrupt i NVIC for TIM6_DAC_IRQn **
  HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : PLAY_Pin NEXT_Pin */
  GPIO_InitStruct.Pin = PLAY_Pin|NEXT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI1_CS_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
  while (1)
  {
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
