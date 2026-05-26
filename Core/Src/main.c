/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * HTS221 + NanoEdge AI + 3-Pin Buzzer Module on PA0
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "NanoEdgeAI.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HTS221_ADDR            (0x5F << 1)
#define HTS221_WHO_AM_I        0x0F
#define HTS221_AV_CONF         0x10
#define HTS221_CTRL_REG1       0x20
#define HTS221_HUMIDITY_OUT_L  0x28
#define HTS221_TEMP_OUT_L      0x2A

#define LEARNING_ITERATIONS    24

#define BUZZER_PORT            GPIOA
#define BUZZER_PIN             GPIO_PIN_0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;
UART_HandleTypeDef huart1;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C2_Init(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint8_t HTS221_Read8(uint8_t reg)
{
  uint8_t val = 0;
  HAL_I2C_Mem_Read(&hi2c2, HTS221_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
  return val;
}

static int16_t HTS221_Read16(uint8_t reg_l)
{
  uint8_t data[2] = {0};
  HAL_I2C_Mem_Read(&hi2c2, HTS221_ADDR, reg_l | 0x80, I2C_MEMADD_SIZE_8BIT, data, 2, 100);
  return (int16_t)((data[1] << 8) | data[0]);
}

static void UART_Print(char *msg)
{
  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
}

/* -------- 3-pin buzzer control --------
   For most 3-pin buzzer modules:
   HIGH  = buzzer ON
   LOW   = buzzer OFF
*/
static void Buzzer_On(void)
{
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

static void Buzzer_Off(void)
{
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

/* Simple beep for 3-pin active buzzer module */
static void Buzzer_Beep(uint16_t on_ms, uint16_t off_ms, uint8_t repeat)
{
  for (uint8_t i = 0; i < repeat; i++)
  {
    Buzzer_On();
    HAL_Delay(on_ms);
    Buzzer_Off();
    HAL_Delay(off_ms);
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_I2C2_Init();

  char buffer[220];

  /* ---------- Startup message ---------- */
  sprintf(buffer, "System Booting...\r\n");
  UART_Print(buffer);

  /* Startup buzzer test */
  Buzzer_Beep(200, 150, 2);

  /* ---------- HTS221 setup ---------- */
  uint8_t who = HTS221_Read8(HTS221_WHO_AM_I);
  sprintf(buffer, "HTS221 WHO_AM_I: 0x%02X\r\n", who);
  UART_Print(buffer);

  uint8_t av_conf = 0x1B;
  HAL_I2C_Mem_Write(&hi2c2, HTS221_ADDR, HTS221_AV_CONF, I2C_MEMADD_SIZE_8BIT, &av_conf, 1, 100);

  uint8_t ctrl_reg1 = 0x85; /* PD=1, BDU=1, ODR=1Hz */
  HAL_I2C_Mem_Write(&hi2c2, HTS221_ADDR, HTS221_CTRL_REG1, I2C_MEMADD_SIZE_8BIT, &ctrl_reg1, 1, 100);

  HAL_Delay(100);

  /* ---------- Read HTS221 calibration ---------- */
  uint8_t H0_rH_x2       = HTS221_Read8(0x30);
  uint8_t H1_rH_x2       = HTS221_Read8(0x31);
  uint8_t T0_degC_x8_lsb = HTS221_Read8(0x32);
  uint8_t T1_degC_x8_lsb = HTS221_Read8(0x33);
  uint8_t T0_T1_msb      = HTS221_Read8(0x35);

  uint16_t T0_degC_x8 = ((uint16_t)(T0_T1_msb & 0x03) << 8) | T0_degC_x8_lsb;
  uint16_t T1_degC_x8 = ((uint16_t)((T0_T1_msb >> 2) & 0x03) << 8) | T1_degC_x8_lsb;

  int16_t H0_T0_OUT = HTS221_Read16(0x36);
  int16_t H1_T0_OUT = HTS221_Read16(0x3A);
  int16_t T0_OUT    = HTS221_Read16(0x3C);
  int16_t T1_OUT    = HTS221_Read16(0x3E);

  int32_t H0_x10 = (int32_t)H0_rH_x2 * 5;
  int32_t H1_x10 = (int32_t)H1_rH_x2 * 5;
  int32_t T0_x10 = ((int32_t)T0_degC_x8 * 10) / 8;
  int32_t T1_x10 = ((int32_t)T1_degC_x8 * 10) / 8;

  sprintf(buffer,
          "Monitoring Started\r\n"
          "H0=%ld.%ld%% H1=%ld.%ld%% T0=%ld.%ldC T1=%ld.%ldC\r\n",
          H0_x10 / 10, labs(H0_x10 % 10),
          H1_x10 / 10, labs(H1_x10 % 10),
          T0_x10 / 10, labs(T0_x10 % 10),
          T1_x10 / 10, labs(T1_x10 % 10));
  UART_Print(buffer);

  /* ---------- NanoEdge AI setup ---------- */
  enum neai_state neai_state;
  uint8_t similarity = 0;

  float input_signal[NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER];
  float temp_hist[NEAI_INPUT_SIGNAL_LENGTH];
  float hum_hist[NEAI_INPUT_SIGNAL_LENGTH];

  uint16_t sample_count = 0;
  uint16_t learning_count = 0;

  for (uint16_t i = 0; i < NEAI_INPUT_SIGNAL_LENGTH; i++)
  {
    temp_hist[i] = 0.0f;
    hum_hist[i]  = 0.0f;
  }

  neai_state = neai_anomalydetection_init(false);
  sprintf(buffer, "NEAI init state: %d\r\n", neai_state);
  UART_Print(buffer);

  while (1)
  {
    /* ---------- Read current sensor values ---------- */
    int16_t H_OUT = HTS221_Read16(HTS221_HUMIDITY_OUT_L);
    int16_t T_OUT = HTS221_Read16(HTS221_TEMP_OUT_L);

    int32_t humidity_x10 = 0;
    int32_t temp_x10 = 0;

    if (H1_T0_OUT != H0_T0_OUT)
    {
      humidity_x10 = ((int32_t)(H_OUT - H0_T0_OUT) * (H1_x10 - H0_x10)) /
                     (H1_T0_OUT - H0_T0_OUT) + H0_x10;
    }

    if (T1_OUT != T0_OUT)
    {
      temp_x10 = ((int32_t)(T_OUT - T0_OUT) * (T1_x10 - T0_x10)) /
                 (T1_OUT - T0_OUT) + T0_x10;
    }

    if (humidity_x10 < 0) humidity_x10 = 0;
    if (humidity_x10 > 1000) humidity_x10 = 1000;

    float temperature = (float)temp_x10 / 10.0f;
    float humidity    = (float)humidity_x10 / 10.0f;

    sprintf(buffer,
            "H_RAW:%d  T_RAW:%d  Temperature:%ld.%ld C  Humidity:%ld.%ld %%\r\n",
            H_OUT, T_OUT,
            temp_x10 / 10, labs(temp_x10 % 10),
            humidity_x10 / 10, labs(humidity_x10 % 10));
    UART_Print(buffer);

    /* ---------- Update rolling history ---------- */
    for (uint16_t i = 0; i < NEAI_INPUT_SIGNAL_LENGTH - 1; i++)
    {
      temp_hist[i] = temp_hist[i + 1];
      hum_hist[i]  = hum_hist[i + 1];
    }

    temp_hist[NEAI_INPUT_SIGNAL_LENGTH - 1] = temperature;
    hum_hist[NEAI_INPUT_SIGNAL_LENGTH - 1]  = humidity;

    /* ---------- Warm-up ---------- */
    if (sample_count < NEAI_INPUT_SIGNAL_LENGTH)
    {
      sample_count++;

      sprintf(buffer, "AI Warming Up: %u/%u\r\n",
              sample_count, NEAI_INPUT_SIGNAL_LENGTH);
      UART_Print(buffer);

      Buzzer_Off();
      HAL_Delay(1000);
      continue;
    }

    /* ---------- Create NanoEdge input buffer ---------- */
    for (uint16_t i = 0; i < NEAI_INPUT_SIGNAL_LENGTH; i++)
    {
      input_signal[(i * NEAI_INPUT_AXIS_NUMBER) + 0] = temp_hist[i];
      input_signal[(i * NEAI_INPUT_AXIS_NUMBER) + 1] = hum_hist[i];
    }

    /* ---------- Learning phase ---------- */
    if (learning_count < LEARNING_ITERATIONS)
    {
      neai_state = neai_anomalydetection_learn(input_signal);
      learning_count++;

      sprintf(buffer, "AI Learning: %u/%u  state=%d\r\n",
              learning_count, LEARNING_ITERATIONS, neai_state);
      UART_Print(buffer);

      Buzzer_Off();
    }
    /* ---------- Detection phase ---------- */
    else
    {
      neai_state = neai_anomalydetection_detect(input_signal, &similarity);

      if (neai_state == NEAI_OK)
      {
        if (similarity >= 90)
        {
          sprintf(buffer, "AI Similarity: %u  AI Status: NORMAL\r\n", similarity);
          UART_Print(buffer);
          Buzzer_Off();
        }
        else
        {
          sprintf(buffer, "AI Similarity: %u  AI Status: ANOMALY DETECTED\r\n", similarity);
          UART_Print(buffer);

          /* Buzzer sounds when anomaly is detected */
          Buzzer_Beep(400, 200, 3);

          HAL_Delay(300);
          continue;
        }
      }
      else
      {
        sprintf(buffer, "AI detect state error: %d\r\n", neai_state);
        UART_Print(buffer);
        Buzzer_Off();
      }
    }

    HAL_Delay(1000);
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x30A175AB;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */0
