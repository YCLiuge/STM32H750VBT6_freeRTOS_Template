#include "w25q64.h"

static bool g_w25q64_present = false;

static bool W25Q64_WriteEnable(void);
static bool W25Q64_WaitBusy(uint32_t timeout_ms);

bool W25Q64_Init(void)
{
  W25Q64_Reset();
  uint32_t id = W25Q64_ReadID();
  uint16_t dev_id = (uint16_t)(id & 0xFFFFU);

  if (dev_id == W25Q64_DEVICE_ID)
  {
    g_w25q64_present = true;
    return true;
  }

  g_w25q64_present = false;
  return false;
}

bool W25Q64_Reset(void)
{
  QSPI_CommandTypeDef sCmd = {0};

  sCmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCmd.Instruction        = 0x66;
  sCmd.AddressMode        = QSPI_ADDRESS_NONE;
  sCmd.DataMode           = QSPI_DATA_NONE;
  sCmd.DummyCycles        = 0;
  if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return false;

  sCmd.Instruction = 0x99;
  if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return false;

  HAL_Delay(50);
  return true;
}

uint32_t W25Q64_ReadID(void)
{
  uint8_t rx[3] = {0};
  QSPI_CommandTypeDef sCmd = {0};

  sCmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCmd.Instruction        = 0x9F;
  sCmd.AddressMode        = QSPI_ADDRESS_NONE;
  sCmd.DataMode           = QSPI_DATA_1_LINE;
  sCmd.DummyCycles        = 0;
  sCmd.NbData             = 3;

  if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return 0U;

  if (HAL_QSPI_Receive(&hqspi, rx, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return 0U;

  return ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | rx[2];
}

bool W25Q64_IsPresent(void)
{
  return g_w25q64_present;
}

uint32_t W25Q64_GetCapacity(void)
{
  return W25Q64_CAPACITY;
}

const char* W25Q64_GetName(void)
{
  return "W25Q64 (8 MB)";
}

bool W25Q64_EraseSector(uint32_t addr)
{
  QSPI_CommandTypeDef sCmd = {0};

  if (!g_w25q64_present) return false;
  if (!W25Q64_WriteEnable()) return false;

  sCmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCmd.Instruction        = 0x20;
  sCmd.AddressMode        = QSPI_ADDRESS_1_LINE;
  sCmd.AddressSize        = QSPI_ADDRESS_24_BITS;
  sCmd.Address             = addr;
  sCmd.DataMode           = QSPI_DATA_NONE;
  sCmd.DummyCycles        = 0;

  if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return false;

  return W25Q64_WaitBusy(500U);
}

bool W25Q64_EraseBlock(uint32_t addr)
{
  QSPI_CommandTypeDef sCmd = {0};

  if (!g_w25q64_present) return false;
  if (!W25Q64_WriteEnable()) return false;

  sCmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCmd.Instruction        = 0xD8;
  sCmd.AddressMode        = QSPI_ADDRESS_1_LINE;
  sCmd.AddressSize        = QSPI_ADDRESS_24_BITS;
  sCmd.Address             = addr;
  sCmd.DataMode           = QSPI_DATA_NONE;
  sCmd.DummyCycles        = 0;

  if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return false;

  return W25Q64_WaitBusy(3000U);
}

bool W25Q64_ChipErase(void)
{
  QSPI_CommandTypeDef sCmd = {0};

  if (!g_w25q64_present) return false;
  if (!W25Q64_WriteEnable()) return false;

  sCmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCmd.Instruction        = 0xC7;
  sCmd.AddressMode        = QSPI_ADDRESS_NONE;
  sCmd.DataMode           = QSPI_DATA_NONE;
  sCmd.DummyCycles        = 0;

  if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return false;

  return W25Q64_WaitBusy(60000U);
}

bool W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
  QSPI_CommandTypeDef sCmd = {0};

  if (!g_w25q64_present || (buf == NULL) || (len == 0U)) return false;

  sCmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCmd.Instruction        = 0x03;
  sCmd.AddressMode        = QSPI_ADDRESS_1_LINE;
  sCmd.AddressSize        = QSPI_ADDRESS_24_BITS;
  sCmd.Address             = addr;
  sCmd.DataMode           = QSPI_DATA_1_LINE;
  sCmd.DummyCycles        = 0;
  sCmd.NbData             = len;

  if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    return false;

  return (HAL_QSPI_Receive(&hqspi, buf, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK);
}

bool W25Q64_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
  QSPI_CommandTypeDef sCmd = {0};
  uint32_t remaining = len;
  uint32_t offset = 0U;

  if (!g_w25q64_present || (buf == NULL) || (len == 0U)) return false;

  while (remaining > 0U)
  {
    uint32_t chunk = remaining;
    if (chunk > W25Q64_PAGE_SIZE) chunk = W25Q64_PAGE_SIZE;

    if (!W25Q64_WriteEnable()) return false;

    sCmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    sCmd.Instruction         = 0x02;
    sCmd.AddressMode         = QSPI_ADDRESS_1_LINE;
    sCmd.AddressSize         = QSPI_ADDRESS_24_BITS;
    sCmd.Address             = addr + offset;
    sCmd.DataMode            = QSPI_DATA_1_LINE;
    sCmd.DummyCycles         = 0;
    sCmd.NbData             = chunk;

    if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return false;

    if (HAL_QSPI_Transmit(&hqspi, (uint8_t *)&buf[offset], HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return false;

    if (!W25Q64_WaitBusy(10U)) return false;

    remaining -= chunk;
    offset    += chunk;
  }

  return true;
}

static bool W25Q64_WriteEnable(void)
{
  QSPI_CommandTypeDef sCmd = {0};

  sCmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCmd.Instruction        = 0x06;
  sCmd.AddressMode        = QSPI_ADDRESS_NONE;
  sCmd.DataMode           = QSPI_DATA_NONE;
  sCmd.DummyCycles        = 0;

  return (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK);
}

static bool W25Q64_WaitBusy(uint32_t timeout_ms)
{
  uint8_t sr;
  QSPI_CommandTypeDef sCmd = {0};
  uint32_t start = HAL_GetTick();

  sCmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCmd.Instruction        = 0x05;
  sCmd.AddressMode        = QSPI_ADDRESS_NONE;
  sCmd.DataMode           = QSPI_DATA_1_LINE;
  sCmd.DummyCycles        = 0;
  sCmd.NbData             = 1;

  do
  {
    if (HAL_QSPI_Command(&hqspi, &sCmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return false;

    sr = 0U;
    if (HAL_QSPI_Receive(&hqspi, &sr, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
      return false;

    if ((sr & 0x01U) == 0U)
      return true;

    HAL_Delay(1);
  } while ((HAL_GetTick() - start) < timeout_ms);

  return false;
}
