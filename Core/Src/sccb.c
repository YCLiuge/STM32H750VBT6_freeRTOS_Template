/***
 * SCCB 协议实现 — 模拟 I2C 接口
 *
 * 1. SCCB 与 IIC 协议兼容
 * 2. 使用 GPIO 模拟接口
 * 3. 通讯速度约 300KHz，不超过 400KHz
 *
 * 参考：
 *   OV2640 器件地址: 0x60 (8位写地址)
 *   OV5640 器件地址: 0x78 (8位写地址)
 ***/

#include "sccb.h"

/*****************************************************************************************
 *  函 数 名: SCCB_GPIO_Config
 *  入口参数: 无
 *  返 回 值: 无
 *  功能描述: 初始化 SCCB 总线的 GPIO
 *  说   明: 由于 SCCB 总线通讯速度不高，GPIO 速度等级设为 Low
 ******************************************************************************************/
void SCCB_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    SCCB_SCL_CLK_ENABLE;
    SCCB_SDA_CLK_ENABLE;

    /* SCL — 开漏输出 */
    GPIO_InitStruct.Pin   = SCCB_SCL_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SCCB_SCL_PORT, &GPIO_InitStruct);

    /* SDA — 开漏输出 */
    GPIO_InitStruct.Pin   = SCCB_SDA_PIN;
    HAL_GPIO_Init(SCCB_SDA_PORT, &GPIO_InitStruct);

    /* 总线空闲状态 — SCL/SDA 高电平 */
    HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_SET);
}

/*****************************************************************************************
 *  函 数 名: SCCB_Delay
 *  入口参数: a — 延时参数
 *  返 回 值: 无
 *  功能描述: 简单延时函数（循环计数，非精确）
 *  说   明: 为方便移植，不依赖定时器
 ******************************************************************************************/
void SCCB_Delay(uint32_t a)
{
    volatile uint16_t i;
    while (a--)
    {
        for (i = 0; i < 6; i++);
    }
}

/*****************************************************************************************
 *  函 数 名: SCCB_Start
 *  入口参数: 无
 *  返 回 值: 无
 *  功能描述: IIC 起始信号
 *  说   明: SCL 高电平期间，SDA 由高变低即为起始信号
 ******************************************************************************************/
void SCCB_Start(void)
{
    SCCB_SDA(1);
    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);

    SCCB_SDA(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
 *  函 数 名: SCCB_Stop
 *  入口参数: 无
 *  返 回 值: 无
 *  功能描述: IIC 停止信号
 *  说   明: SCL 高电平期间，SDA 由低变高即为停止信号
 ******************************************************************************************/
void SCCB_Stop(void)
{
    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SDA(0);
    SCCB_Delay(SCCB_DelayVaule);

    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SDA(1);
    SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
 *  函 数 名: SCCB_ACK
 *  入口参数: 无
 *  返 回 值: 无
 *  功能描述: IIC 应答信号 (Master → Slave)
 *  说   明: SCL 高电平期间，SDA 输出低电平表示应答
 ******************************************************************************************/
void SCCB_ACK(void)
{
    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SDA(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);

    SCCB_SCL(0);       /* SCL 拉低后释放 SDA */
    SCCB_SDA(1);
    SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
 *  函 数 名: SCCB_NoACK
 *  入口参数: 无
 *  返 回 值: 无
 *  功能描述: IIC 非应答信号 (Master → Slave)
 *  说   明: SCL 高电平期间，SDA 保持高电平表示非应答
 ******************************************************************************************/
void SCCB_NoACK(void)
{
    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SDA(1);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);

    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
 *  函 数 名: SCCB_WaitACK
 *  入口参数: 无
 *  返 回 值: ACK_OK — 设备应答正常; ACK_ERR — 设备未应答
 *  功能描述: 等待从设备返回应答信号
 *  说   明: SCL 高电平期间，检测 SDA 是否为低电平来判断设备是否应答
 ******************************************************************************************/
uint8_t SCCB_WaitACK(void)
{
    SCCB_SDA(1);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);

    if (HAL_GPIO_ReadPin(SCCB_SDA_PORT, SCCB_SDA_PIN) != 0)
    {
        SCCB_SCL(0);
        SCCB_Delay(SCCB_DelayVaule);
        return ACK_ERR;   /* 设备未应答 */
    }
    else
    {
        SCCB_SCL(0);
        SCCB_Delay(SCCB_DelayVaule);
        return ACK_OK;    /* 应答正常 */
    }
}

/*****************************************************************************************
 *  函 数 名: SCCB_WriteByte
 *  入口参数: IIC_Data — 要写入的 8 位数据
 *  返 回 值: ACK_OK / ACK_ERR
 *  功能描述: 写入一个字节数据
 *  说   明: 高位在前
 ******************************************************************************************/
uint8_t SCCB_WriteByte(uint8_t IIC_Data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        SCCB_SDA(IIC_Data & 0x80);

        SCCB_Delay(SCCB_DelayVaule);
        SCCB_SCL(1);
        SCCB_Delay(SCCB_DelayVaule);
        SCCB_SCL(0);
        if (i == 7)
        {
            SCCB_SDA(1);   /* 释放 SDA 总线供从设备应答 */
        }
        IIC_Data <<= 1;
    }

    return SCCB_WaitACK();
}

/*****************************************************************************************
 *  函 数 名: SCCB_ReadByte
 *  入口参数: ACK_Mode — 1=发送应答, 0=发送非应答
 *  返 回 值: 读取到的 8 位数据
 *  功能描述: 读取一个字节数据
 *  说   明: 1. 高位在前
 *           2. 最后一个字节读取时发送非应答信号
 ******************************************************************************************/
uint8_t SCCB_ReadByte(uint8_t ACK_Mode)
{
    uint8_t IIC_Data = 0;
    uint8_t i = 0;

    for (i = 0; i < 8; i++)
    {
        IIC_Data <<= 1;

        SCCB_SCL(1);
        SCCB_Delay(SCCB_DelayVaule);
        IIC_Data |= (HAL_GPIO_ReadPin(SCCB_SDA_PORT, SCCB_SDA_PIN) & 0x01);
        SCCB_SCL(0);
        SCCB_Delay(SCCB_DelayVaule);
    }

    if (ACK_Mode == 1)
        SCCB_ACK();
    else
        SCCB_NoACK();

    return IIC_Data;
}


/*****************************************************************************************
 *  函 数 名: SCCB_WriteHandle  (8位地址 — OV2640 用)
 *  入口参数: addr — 要操作的寄存器 (8位地址)
 *  返 回 值: SUCCESS / ERROR
 *  功能描述: 对指定寄存器执行写操作句柄
 ******************************************************************************************/
uint8_t SCCB_WriteHandle(uint8_t addr)
{
    uint8_t status = ERROR;

    SCCB_Start();
    if (SCCB_WriteByte(OV2640_DEVICE_ADDRESS) == ACK_OK)
    {
        if (SCCB_WriteByte((uint8_t)(addr)) == ACK_OK)
        {
            status = SUCCESS;
        }
    }
    return status;
}

/*****************************************************************************************
 *  函 数 名: SCCB_WriteReg  (8位地址 — OV2640 用)
 *  入口参数: addr — 要写入的寄存器; value — 要写入的数据
 *  返 回 值: SUCCESS / ERROR
 *  功能描述: 向指定寄存器写入一个字节数据
 ******************************************************************************************/
uint8_t SCCB_WriteReg(uint8_t addr, uint8_t value)
{
    uint8_t status = ERROR;

    SCCB_Start();

    if (SCCB_WriteHandle(addr) == SUCCESS)
    {
        if (SCCB_WriteByte(value) == ACK_OK)
        {
            status = SUCCESS;
        }
    }
    SCCB_Stop();

    return status;
}

/*****************************************************************************************
 *  函 数 名: SCCB_ReadReg  (8位地址 — OV2640 用)
 *  入口参数: addr — 要读取的寄存器
 *  返 回 值: 寄存器值
 *  功能描述: 从指定寄存器读取一个字节数据
 ******************************************************************************************/
uint8_t SCCB_ReadReg(uint8_t addr)
{
    uint8_t value = 0;

    SCCB_Start();

    if (SCCB_WriteHandle(addr) == SUCCESS)
    {
        SCCB_Stop();
        SCCB_Start();

        if (SCCB_WriteByte(OV2640_DEVICE_ADDRESS | 0x01) == ACK_OK)
        {
            value = SCCB_ReadByte(0);   /* 读取最后一个字节，发送非应答 */
        }
        SCCB_Stop();
    }

    return value;
}


/*****************************************************************************************
 *  函 数 名: SCCB_WriteHandle_16Bit  (16位地址 — OV5640 用)
 *  入口参数: addr — 要操作的寄存器 (16位地址)
 *  返 回 值: SUCCESS / ERROR
 *  功能描述: 对指定寄存器执行写操作句柄 (16位地址版本)
 ******************************************************************************************/
uint8_t SCCB_WriteHandle_16Bit(uint16_t addr)
{
    uint8_t status = ERROR;

    SCCB_Start();
    if (SCCB_WriteByte(OV5640_DEVICE_ADDRESS) == ACK_OK)
    {
        if (SCCB_WriteByte((uint8_t)(addr >> 8)) == ACK_OK)
        {
            if (SCCB_WriteByte((uint8_t)(addr)) == ACK_OK)
            {
                status = SUCCESS;
            }
        }
    }
    return status;
}

/*****************************************************************************************
 *  函 数 名: SCCB_WriteReg_16Bit  (16位地址 — OV5640 用)
 *  入口参数: addr — 要写入的寄存器; value — 要写入的数据
 *  返 回 值: SUCCESS / ERROR
 *  功能描述: 向指定寄存器写入一个字节数据 (16位地址版本)
 ******************************************************************************************/
uint8_t SCCB_WriteReg_16Bit(uint16_t addr, uint8_t value)
{
    uint8_t status = ERROR;

    SCCB_Start();

    if (SCCB_WriteHandle_16Bit(addr) == SUCCESS)
    {
        if (SCCB_WriteByte(value) == ACK_OK)
        {
            status = SUCCESS;
        }
    }
    SCCB_Stop();

    return status;
}

/*****************************************************************************************
 *  函 数 名: SCCB_ReadReg_16Bit  (16位地址 — OV5640 用)
 *  入口参数: addr — 要读取的寄存器
 *  返 回 值: 寄存器值
 *  功能描述: 从指定寄存器读取一个字节数据 (16位地址版本)
 ******************************************************************************************/
uint8_t SCCB_ReadReg_16Bit(uint16_t addr)
{
    uint8_t value = 0;

    SCCB_Start();

    if (SCCB_WriteHandle_16Bit(addr) == SUCCESS)
    {
        SCCB_Stop();
        SCCB_Start();

        if (SCCB_WriteByte(OV5640_DEVICE_ADDRESS | 0x01) == ACK_OK)
        {
            value = SCCB_ReadByte(0);
        }
        SCCB_Stop();
    }

    return value;
}

/*****************************************************************************************
 *  函 数 名: SCCB_WriteBuffer_16Bit  (16位地址 — OV5640 用)
 *  入口参数: addr — 起始寄存器; pData — 数据缓冲区; size — 数据大小
 *  返 回 值: SUCCESS / ERROR
 *  功能描述: 向指定寄存器连续写入多字节数据 (OV5640 写自动对焦固件时用)
 ******************************************************************************************/
uint8_t SCCB_WriteBuffer_16Bit(uint16_t addr, uint8_t *pData, uint32_t size)
{
    uint8_t status = ERROR;
    uint32_t i;

    SCCB_Start();

    if (SCCB_WriteHandle_16Bit(addr) == SUCCESS)
    {
        status = SUCCESS;         /* 句柄成功即视为整体成功 */
        for (i = 0; i < size; i++)
        {
            (void)SCCB_WriteByte(*pData);
            pData++;
        }
    }
    SCCB_Stop();

    return status;
}
