# STM32H750VBT6 FreeRTOS 快速开发模板

## 1. 项目概述

基于 STM32H750VBT6（Cortex-M7@480MHz）的 FreeRTOS 固件模板。
设计目标：**高内聚、低耦合**的服务模块化架构，可快速移植到其他 STM32H7 项目。

**当前运行时功能：**
- 双通道 ADC DMA 连续采样（前向/反向功率传感器）
- 内部温度 / VDDA 电压监测（ADC3 轮询）
- USB CDC 虚拟串口 CLI 命令行交互（10 条诊断命令）
- OLED / LCD 双显示支持（编译开关切换）
- OV2640 摄像头 DCMI 采集（USB CDC 实时验证）
- 系统健康监控（堆栈水位、FreeRTOS 运行时统计）

**架构分层：**

```
应用层 (app_*.c)        ← 手写业务逻辑
  ↕ app.h 统一接口
平台层 (freertos.c)      ← CubeMX 生成，Handler 转发到应用层
中间件 (FreeRTOS / USB CDC / FatFs / FreeRTOS-Plus-CLI)
HAL   (ADC / DMA / SPI / DCMI / TIM / ...)
BSP   (CubeMX 生成的外设初始化)
```

---

## 2. 目录结构

```
├── Core/
│   ├── Inc/
│   │   ├── app.h                    ← 统一 API 中心 + DTO 定义
│   │   ├── app_*.h                  ← 兼容包装头（均 #include "app.h"）
│   │   ├── app_camera.h             ← 摄像头模块兼容头
│   │   ├── display_service.h        ← 显示抽象层
│   │   ├── lcd_st7789.h             ← ST7789 LCD 驱动接口（独立模块）
│   │   ├── main.h                   ← 系统引脚定义（CubeMX 生成）
│   │   ├── oled_spi_V0.2.h          ← SSD1306 OLED 驱动宏
│   │   ├── spi.h                    ← SPI4 外设声明（CubeMX 生成）
│   │   ├── FreeRTOSConfig.h         ← FreeRTOS 配置
│   │   ├── dcmi.h / dcmi_ov2640_cfg.h / sccb.h  ← DCMI/摄像头驱动
│   │   ├── lcd_model.h / lcd_fonts.h / lcd_image.h  ← LCD 辅助模块
│   │   └── VSWR.h / sdmmc.h / ff.h ...  ← 可选功能模块头文件
│   ├── Src/
│   │   ├── main.c                   ← 入口：时钟/MPU/外设初始化 → 启动 RTOS
│   │   ├── freertos.c               ← RTOS 对象创建 + Handler 转发
│   │   ├── app_adc.c                ← ADC 服务：DMA 管理/平均/温度/VREF
│   │   ├── app_console.c            ← 控制台服务：Stream Buffer RX / CLI
│   │   ├── app_sysmon.c             ← 系统监控：快照 / 显示刷新 / 运行时统计
│   │   ├── app_init.c               ← 启动编排：USB / ADC 初始化
│   │   ├── app_rtos_hooks.c         ← FreeRTOS 钩子（栈溢出/内存耗尽/运行时统计）
│   │   ├── app_camera.c             ← 摄像头服务：OV2640 初始化/捕获/诊断
│   │   ├── display_service.c        ← 显示抽象实现（OLED / LCD 编译时选择）
│   │   ├── lcd_st7789.c             ← ST7789 LCD 驱动实现（独立模块）
│   │   ├── oled_spi_V0.2.c          ← SSD1306 OLED 驱动（SPI4, 128×64）
│   │   ├── spi.c                    ← CubeMX SPI4 外设初始化
│   │   ├── dcmi.c                   ← CubeMX DCMI + OV2640 驱动（USER CODE 区）
│   │   ├── sccb.c                   ← SCCB（I2C-like）摄像头通信协议
│   │   ├── stm32h7xx_it.c           ← 中断服务函数（CubeMX 生成）
│   │   ├── stm32h7xx_hal_msp.c      ← HAL MSP 初始化
│   │   ├── FreeRTOS_CLI.c           ← FreeRTOS-Plus-CLI 命令行解析器
│   │   ├── lcd_model.c / lcd_fonts.c / lcd_image.c  ← LCD 图形/字体/图片
│   │   └── adc.c, gpio.c, dma.c, tim.c, usart.c ... ← CubeMX 外设初始化
├── Drivers/                         ← CMSIS + STM32H7 HAL
├── Middlewares/                      ← FreeRTOS Kernel + STM32 USB Device Library
├── USB_DEVICE/                       ← USB CDC 描述符与接口
│   └── App/usbd_cdc_if.c/h          ← CDC 收发接口（USER CODE 区增强）
├── FatFs/                            ← FatFs R0.14b 文件系统 + SD 卡驱动
├── MDK-ARM/                          ← Keil uVision 工程 + 散列文件
│   ├── STM32H750VBT6_freeRTOS_Template.uvprojx
│   └── STM32H750VBT6_freeRTOS_Template.sct  ← 链接脚本（内存布局）
└── STM32H750VBT6_freeRTOS_Template.ioc  ← CubeMX 工程配置
```

---

## 3. 文件分类与修改规则

| 类型 | 标识 | 修改规则 |
|------|------|----------|
| **手写模块** | 不含 `USER CODE BEGIN/END` 标记 | 自由修改 |
| **CubeMX 生成** | 含 `USER CODE BEGIN/END` 标记 | **仅可修改标记区域内代码**<br>重新生成代码时区域外会被覆盖 |
| **第三方/ST 库** | FatFs / FreeRTOS / ST HAL | 不修改（除非明确知悉影响） |

### 3.1 手写模块清单（可自由修改）

| 文件 | 职责 |
|------|------|
| `app.h` | 统一 API 中心：公共类型、跨模块函数声明、RTOS 对象导出 |
| `app_adc.c` | ADC1/2 DMA 采集+平均、ADC3 温度/VREF 轮询、Cache 一致性 |
| `app_console.c` | USB CDC CLI、StreamBuffer RX、命令注册与处理 |
| `app_sysmon.c` | 系统快照、堆栈水位、LCD 刷新、运行时统计报告 |
| `app_init.c` | 启动编排：USB/ADC 初始化、首次快照、自删除 |
| `app_rtos_hooks.c` | FreeRTOS Hook 覆盖（栈溢出/内存耗尽→OLED 报错、运行时统计） |
| `app_camera.c` | OV2640 摄像头服务：初始化/快照/连续捕获/hex dump/像素采样/图像统计 |
| `display_service.c` | 显示抽象层：编译开关选择 OLED 或 LCD |
| `lcd_st7789.c` | ST7789 LCD 驱动（2100+ 行） |
| `lcd_model.c` | LCD 图形测试函数 |
| `lcd_fonts.c` | LCD 字体字模数据 |
| `lcd_image.c` | LCD 图片数据 |
| `oled_spi_V0.2.c` | SSD1306 OLED 驱动 |
| `sccb.c` | SCCB 协议（OV2640 摄像头配置） |
| `VSWR.c` | VSWR 驻波比计算 |
| `FreeRTOS_CLI.c` | FreeRTOS-Plus-CLI 命令行解析器 |

### 3.2 CubeMX 生成文件（仅 USER CODE 区域可修改）

| 文件 | 生成内容 | USER CODE 区域手写内容 |
|------|----------|------------------------|
| `main.c` | MPU/时钟/外设初始化 | 显示初始化、ADC 校准、MPU Region 2（Camera DMA）、状态文本 |
| `freertos.c` | 任务/互斥锁/队列定义 | `#include "app.h"`、Handler 转发、`HAL_ADC_ConvCpltCallback` 桥接 |
| `stm32h7xx_hal_msp.c` | HAL 全局 MSP 初始化 | `ExitRun0Mode()` 空实现（startup 调用） |
| `stm32h7xx_it.c` | 中断服务函数 | `#include "dcmi.h"` |
| `dcmi.c` | DCMI 外设初始化 | 全部 OV2640 驱动函数（USER CODE 1） |
| `spi.c` | SPI4 外设初始化 | —（LCD 代码已迁移至 `lcd_st7789.c`） |
| `adc.c` / `dma.c` / `gpio.c` / `tim.c` | HAL 外设初始化 | — |
| `spi.h` | SPI4 声明 | `LCD_Width=240` / `LCD_Height=280`（dcmi.h 依赖） |
| `dcmi.h` | DCMI 声明 | `#include "sccb.h"` / `#include "lcd_st7789.h"` / 摄像头宏 |

### 3.3 USB CDC 增强（CubeMX 生成 + USER CODE）

| 文件 | USER CODE 手写内容 |
|------|---------------------|
| `USB_DEVICE/App/usbd_cdc_if.h` | `CDC_IsReady_FS()` 声明、`Console_RxPushFromISR()` 声明 |
| `USB_DEVICE/App/usbd_cdc_if.c` | `CDC_IsReady_FS()` 设备就绪检测、`CDC_Transmit_FS()` NULL 防御+Busy 检测、`CDC_Receive_FS()` → ISR 推入 StreamBuffer |

---

## 4. 模块导入指南

新项目或从本模板移植时需要完成：**CubeMX 配置 → Keil 工程设置 → 代码添加**。

### 4.1 基础模块（必须，所有项目通用）

#### 4.1.1 FreeRTOS + CMSIS-RTOS2

| 步骤 | 操作 |
|------|------|
| **CubeMX** | Pinout → **Middleware** → **FREERTOS** → Interface: **CMSIS_V2** |
| | Config parameters → `TOTAL_HEAP_SIZE`: 15360, `TICK_RATE_HZ`: 1000 |
| | Include parameters → `GENERATE_RUN_TIME_STATS`: ☑, `USE_TRACE_FACILITY`: ☑, `USE_STATS_FORMATTING_FUNCTIONS`: ☑ |
| **文件** | `freertos.c`（CubeMX 自动生成）、`FreeRTOSConfig.h`（配置） |
| **Keil** | Middlewares → FreeRTOS → Source → 确保 `tasks.c` / `queue.c` / `timers.c` / `stream_buffer.c` 已添加 |
| **代码** | 在 `freertos.c` 的 `USER CODE BEGIN Includes` 添加 `#include "app.h"` |
| | 各 Handler 函数内调用 `AppXxx_Task(argument)` |

#### 4.1.2 系统时钟

| 步骤 | 操作 |
|------|------|
| **CubeMX** | Pinout → **RCC** → HSE: **Crystal/Ceramic Resonator** / HSI48: ☑ |
| | **Clock Configuration** → HCLK = 480MHz, APB = 240MHz |
| **Keil** | 无需额外操作 |

#### 4.1.3 HAL 时基（TIM6 替代 SysTick）

| 步骤 | 操作 |
|------|------|
| **CubeMX** | Pinout → **System Core** → **SYS** → Timebase Source: **TIM6** |
| **文件** | `stm32h7xx_hal_timebase_tim.c`（CubeMX 自动生成） |
| **Keil** | 自动添加 |

---

### 4.2 OLED 显示模块（SSD1306 128×64, SPI4）

| 步骤 | 操作 |
|------|------|
| **CubeMX** | Pinout → **Connectivity** → **SPI4**: Mode: **Transmit Only Master** |
| | SPI4 → NSS: **Hardware NSS Output** |
| | SPI4 → BaudRatePrescaler: **2**（LCD 60MHz）或 **32**（OLED 3.75MHz，在 `OLED_Init()` 中动态切换） |
| | GPIO 初始化：`PE9`=Output(OLED_RES), `PE10`=Output(OLED_DC) |
| **文件** | `Core/Src/oled_spi_V0.2.c`、`Core/Inc/oled_spi_V0.2.h`、`Core/Inc/oledfont.h` |
| **Keil** | Application/User/Core 组中添加 `oled_spi_V0.2.c` |
| **代码** | `Core/Inc/app.h`: `#define APP_DISPLAY_TYPE DISPLAY_TYPE_OLED` |
| **硬件引脚** | PE12=SCLK, PE14=MOSI, PE11=CS(NSS), PE10=DC, PE9=RST |

### 4.3 LCD 显示模块（ST7789 240×280, SPI4）

| 步骤 | 操作 |
|------|------|
| **CubeMX** | 同 OLED：SPI4 配置，SPI4 BaudRatePrescaler: **2**（60MHz） |
| | GPIO：`PE15`=Output(LCD_DC), `PD15`=Output(LCD_BL) ← 需手动添加 |
| **文件** | `Core/Src/lcd_st7789.c`、`Core/Inc/lcd_st7789.h`、`Core/Src/lcd_fonts.c`、`Core/Inc/lcd_fonts.h`、`Core/Src/lcd_image.c`、`Core/Inc/lcd_image.h`、`Core/Src/lcd_model.c`、`Core/Inc/lcd_model.h` |
| **Keil** | Application/User/Core 组中添加：`lcd_st7789.c`、`lcd_fonts.c`、`lcd_image.c`、`lcd_model.c` |
| **代码** | `Core/Inc/app.h`: `#define APP_DISPLAY_TYPE DISPLAY_TYPE_LCD` |
| **硬件引脚** | PE12=SCLK, PE14=MOSI, PE11=CS(NSS), PE15=DC, PD15=背光 |

### 4.4 显示抽象层

| 步骤 | 操作 |
|------|------|
| **文件** | `Core/Src/display_service.c`、`Core/Inc/display_service.h` |
| **Keil** | Application/User/Core 组中添加 `display_service.c` |
| **代码** | `main.c` USER CODE BEGIN 2 中添加 `DisplayService_Init()` |
| **依赖** | 必须已导入 OLED 或 LCD 驱动之一 |

### 4.5 ADC 服务模块

| 步骤 | 操作 |
|------|------|
| **CubeMX** | Pinout → **Analog** → **ADC1**: IN1 Single-ended, Scan: Disable, Continuous: Enable, DMA: Circular |
| | **ADC2**: IN2 Single-ended, 同上配置 |
| | **ADC3**: Temperature Sensor Channel + Vrefint Channel, Continuous: Disable（轮询模式） |
| | **DMA** → DMA1_Stream0(ADC1) + DMA1_Stream1(ADC2): Circular, Word, Priority: 5 |
| | **NVIC** → DMA1_Stream0/1 中断使能, Priority: 5 |
| **文件** | `Core/Src/app_adc.c`、`Core/Inc/app.h`（含接口声明） |
| **Keil** | Application/User 组中添加 `app_adc.c` |
| **代码** | 在 `app.h` 中添加 `AppAdc_*()` 函数声明 |
| **sct** | `RW_DMA 0x30000000 0x00008000 { *(.bss.dma_buffer) }` |
| **MPU .ioc** | CORTEX_M7 → MPU Region 1: Base=0x30000000, Size=32KB, non-cacheable |

### 4.6 系统监控模块

| 步骤 | 操作 |
|------|------|
| **CubeMX** | **Timers** → **TIM2**: 32-bit 自由运行定时器（运行时统计时间基） |
| **文件** | `Core/Src/app_sysmon.c`、`Core/Src/app_rtos_hooks.c` |
| **Keil** | Application/User 组中添加两个 `.c` 文件 |
| **代码** | `app_rtos_hooks.c` 提供 `configureTimerForRunTimeStats()` / `getRunTimeCounterValue()` |
| | `FreeRTOSConfig.h` USER CODE 2 添加: `#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS configureTimerForRunTimeStats` |
| **依赖** | 需要 app_adc、display_service、app_console 模块 |

### 4.7 USB CDC 控制台模块

| 步骤 | 操作 |
|------|------|
| **CubeMX** | Pinout → **Connectivity** → **USB_OTG_FS**: Mode: **Device_Only** |
| | **Middleware** → **USB_DEVICE** → Class: **Communication Device Class (CDC)** |
| | **NVIC** → OTG_FS 中断使能, Priority: 5 |
| | USB_DEVICE → **CDC** 参数: Rx/ Tx Buffer Size = 2048 |
| **文件** | `Core/Src/app_console.c`、`Core/Src/FreeRTOS_CLI.c` |
| | `USB_DEVICE/App/usbd_cdc_if.c`（CubeMX 生成后需补 USER CODE） |
| | `Core/Inc/FreeRTOS_CLI.h` |
| **Keil** | Application/User 组中添加 `app_console.c`、`FreeRTOS_CLI.c` |
| **代码补丁** | `usbd_cdc_if.h` 的 `USER CODE BEGIN EXPORTED_FUNCTIONS` 添加：<br>`uint8_t CDC_IsReady_FS(void);`<br>`void Console_RxPushFromISR(const uint8_t *buf, uint32_t len);` |
| | `usbd_cdc_if.c` 的 `USER CODE END 6`（CDC_Receive_FS 内）添加：<br>`Console_RxPushFromISR(Buf, *Len);` |
| | `usbd_cdc_if.c` 的 USER CODE BEGIN 7（CDC_Transmit_FS 内）添加：<br>`NULL/0 防御 + CDC_IsReady_FS() 检查 + TxState 检查` |
| | `usbd_cdc_if.c` 末尾 USER CODE 添加：`CDC_IsReady_FS()` 实现 |
| **依赖** | 需要 FreeRTOS Stream Buffer（`stream_buffer.c`） |

### 4.8 摄像头 DCMI 模块（OV2640）

| 步骤 | 操作 |
|------|------|
| **CubeMX** | Pinout → **DCMI**: PCLK=PA6, HSYNC=PA4, VSYNC=PB7, D0=PC6, D1=PC7, D2=PE0, D3=PE1, D4=PE4, D5=PD3, D6=PE5, D7=PE6 |
| | DCMI → **DMA Settings**: 添加 DMA2_Stream7, Direction: P2M, Data Width: Word, Mode: Normal, **Use Fifo**: ☑, Threshold: Full |
| | **NVIC** → DCMI global interrupt: ☑, Priority: 5 |
| | **CORTEX_M7** → MPU Region 2: 暂不配置（当前系统 AXI SRAM 默认 non-cacheable） |
| **文件** | `Core/Src/dcmi.c`（CubeMX 生成 + USER CODE）、`Core/Inc/dcmi.h` |
| | `Core/Src/sccb.c`、`Core/Inc/sccb.h`（SCCB GPIO 模拟 I2C） |
| | `Core/Inc/dcmi_ov2640_cfg.h`（OV2640 寄存器配置表） |
| | `Core/Src/app_camera.c`、`Core/Inc/app_camera.h`（应用层封装） |
| **Keil** | Application/User/Core 组中添加 `sccb.c` |
| | Application/User 组中添加 `app_camera.c` |
| **代码** | `dcmi.h` 的 `USER CODE BEGIN Private defines` 添加：<br>`extern volatile uint8_t DCMI_FrameState;`<br>`extern volatile uint8_t OV2640_FPS;`<br>`#define OV2640_Width 400` / `#define OV2640_Height 300` / `#define Display_Width LCD_Width` ... |
| | `dcmi.h` 的 `USER CODE BEGIN Prototypes` 添加：<br>`void OV2640_DMA_Transmit_Snapshot(...);` 等 |
| | `dcmi.h` 的 `USER CODE BEGIN Includes` 添加：`#include "sccb.h"` / `#include "lcd_st7789.h"` |
| | `stm32h7xx_it.c` 的 `USER CODE BEGIN Includes` 添加：`#include "dcmi.h"` |
| | `stm32h7xx_hal_msp.c` 的 `USER CODE BEGIN 1` 添加 `ExitRun0Mode()` 空实现 |
| **Camera Buffer 内存** | 当前方案：`CameraBuffer` 随 `.ANY` 放置在 AXI SRAM 中<br>AXI SRAM 在 Cortex-M7 默认 non-cacheable，DMA 写入安全<br>*（如需正式启用 Cache 需配置 MPU 子区域隔离）* |
| **硬件引脚** | SCCB: PB8=SCL, PB9=SDA（GPIO 模拟）<br>PWDN: PD14 |

### 4.9 VSWR 驻波比模块（可选，未接入运行时）

| 步骤 | 操作 |
|------|------|
| **CubeMX** | **TIM6**: 已用作 HAL 时基，需改用其他定时器（如 TIM7） |
| | **USART1**: 模式 Asynchronous, PA9=TX, PA10=RX |
| **文件** | `Core/Src/VSWR.c`、`Core/Inc/VSWR.h` |
| **Keil** | Application/User 组中添加 `VSWR.c` |
| **代码** | `main.c` USER CODE 2 取消 `VSWR_Meter_Init()` 注释 |

### 4.10 FatFs + SD 卡模块（可选，未接入运行时）

| 步骤 | 操作 |
|------|------|
| **CubeMX** | Pinout → **Connectivity** → **SDMMC1**: SD 4-bit Wide Bus, ClockDiv=6 |
| | **Middleware** → **FATFS**: Mode: **SD Card**, 其他默认 |
| **文件** | `Core/Src/sd_diskio.c`、`Core/Src/diskio.c`、`FatFs/` 目录下所有文件 |
| **Keil** | 勾选 CubeMX 管理的 FatFs 文件组 |
| **代码** | `main.c` 取消 `MX_SDMMC1_SD_Init()` 注释 |
| **注意** | 未插 SD 卡时 `HAL_SD_Init()` 可能超时阻塞，建议保持注释 |

---

## 5. 头文件架构（Facade 模式）

```
app_xxx.c → app_xxx.h（兼容包装头）→ app.h（统一 API 中心）
             display_service.h ──────→ app.h
             app_camera.h ───────────→ app.h
```

`app.h` 承担 4 类职责：
1. 汇总所有 `app_` 模块间的公共类型定义
2. 汇总所有跨模块调用的公共函数声明
3. 汇总 `freertos.c` 导出的共享 RTOS 对象句柄
4. 汇总 ADC DMA 共享缓冲区等跨模块可见符号

**Include 规则：**
- App 模块之间只包含 `app.h`，**不横向包含**其他 `app_xxx.h`
- 每个 `.c` 文件优先包含自己的 `.h`，再包含本模块真正依赖的外设头文件
- CubeMX 生成文件仅在 `USER CODE` 区域写 `#include "app.h"` 和 `AppXxx_*()` 调用

---

## 6. 任务与资源

| # | 任务名 | 处理函数 | 优先级 | 栈 (words) | 触发方式 |
|---|--------|---------|--------|-----------|----------|
| 1 | Init_Task | `AppInit_Task` | High | 128 | 一次性，自删除 |
| 2 | ADC1_Task | `AppAdc_Task1` | AboveNormal | 128 | DMA 通知（阻塞等待） |
| 3 | ADC2_Task | `AppAdc_Task2` | AboveNormal | 128 | DMA 通知（阻塞等待） |
| 4 | ADC3_Task | `AppAdc_Task3` | BelowNormal | 128 | 周期 1000ms |
| 5 | Console_Task | `AppConsole_Task` | BelowNormal | 1024 | Stream Buffer RX 阻塞 |
| 6 | SysMon_Task | `AppSysMon_Task` | Low | 512 | 周期 200ms（快照 1000ms） |

**RTOS 对象：**
| 类型 | 名称 | 用途 |
|------|------|------|
| Mutex | `UsbTxMutex` | USB CDC 发送串行化 |
| Mutex | `LCDMutex` | 显示访问互斥（Init/SysMon 共享） |
| Queue | `ADCData_QueueHandle` | 预留（当前未使用） |

**FreeRTOS 配置：**
- Heap: 15KB (heap_4)
- Tick: 1000Hz, 56 优先级
- 运行时统计: ☑ (TIM2 32-bit 自由运行定时器)
- 栈溢出检测: ☑ (方法 2)

---

## 7. CLI 命令参考

通过 USB CDC 虚拟串口访问（波特率 115200 或其他均可）：

| 命令 | 功能 |
|------|------|
| `help` | 列出所有命令 |
| `sys` | 系统快照（ADC 值、温度、堆栈水位） |
| `stats` | FreeRTOS 运行时统计（累计 CPU 占比） |
| `statsd` | 两次调用间的运行时增量统计 |
| `all` | `sys` + `stats` 组合输出 |
| `tasks` | 任务状态表 |
| `adc` | ADC 摘要（平均值/温度/VREF/VDDA） |
| `adc raw` | ADC1/ADC2 DMA 缓冲区前 5 个原始值 |
| `camera` | 摄像头子命令（如下） |

**摄像头子命令：**

| 子命令 | 功能 |
|--------|------|
| `camera init` | 初始化 OV2640（SCCB + 寄存器配置） |
| `camera id` | 读取传感器 ID（无需 init） |
| `camera snap` | 抓取单帧快照 |
| `camera cont` | 启动连续捕获模式 |
| `camera stop` | 停止 DCMI 捕获 |
| `camera status` | 显示缓冲区地址/帧状态/FPS |
| `camera hex [offset] [len]` | 摄像头缓冲区 hex dump |
| `camera pixels x y [w] [h]` | 采样指定区域像素 RGB565 值 |
| `camera verify` | 全图统计（min/max/avg/zero%/sat% + 中心 5x5） |

---

## 8. 显示配置

### 8.1 OLED (SSD1306 128×64) — 当前激活

```c
#define APP_DISPLAY_TYPE  DISPLAY_TYPE_OLED   // 在 Core/Inc/app.h
```

| 信号 | 引脚 | 说明 |
|------|------|------|
| SCLK | PE12 (SPI4_SCK) | SPI 时钟 |
| MOSI | PE14 (SPI4_MOSI) | SPI 数据 |
| DC | PE10 (GPIO) | 命令/数据选择 |
| RST | PE9 (GPIO) | 硬件复位（低有效） |
| CS | PE11 (SPI4_NSS) | 硬件 NSS 管理 |

- SPI 时钟：3.75 MHz（`OLED_Init()` 中重配置预分频为 32）
- 字体：6×8 像素，21 字符 × 8 行
- Y 坐标由 `display_service.c` 内部转换为页寻址（y/8）

### 8.2 LCD (ST7789 240×280)

```c
#define APP_DISPLAY_TYPE  DISPLAY_TYPE_LCD    // 在 Core/Inc/app.h
```

| 信号 | 引脚 | 说明 |
|------|------|------|
| SCLK | PE12 (SPI4_SCK) | SPI 时钟 |
| MOSI | PE14 (SPI4_MOSI) | SPI 数据 |
| DC | PE15 (GPIO) | 命令/数据选择 |
| BL | PD15 (GPIO) | 背光控制 |

- SPI 时钟：60 MHz（CubeMX 默认预分频 2）
- 支持多字体 ASCII 和中文、图形绘制、旋转

---

## 9. 内存布局

### 9.1 散列文件（`.sct`）

```
Flash:  0x08000000  128KB   代码 + 只读数据
DTCM:   0x20000000  128KB   启动栈 + 堆
SRAM1:  0x30000000   32KB   ADC DMA non-cacheable (RW_DMA)
AXI:    0x24000000  512KB   全局变量 + FreeRTOS heap + CameraBuffer (RW_AXI)
```

### 9.2 MPU 配置（`.ioc` → CORTEX_M7）

| Region | 基地址 | 大小 | 缓存 | 用途 |
|--------|--------|------|------|------|
| 0 | 0x00000000 | 4GB | Cacheable | 背景区域（SubRegionDisable=0x87 排除部分区域） |
| 1 | 0x30000000 | 32KB | **Non-cacheable** | ADC DMA 缓冲区 |

### 9.3 DMA 缓存一致性

- ADC DMA 缓冲区：`__attribute__((section(".bss.dma_buffer")))` 放置在 0x30000000
- Camera DMA 缓冲区：当前默认放置在 AXI SRAM（0x24000000 区域）
  - Cortex-M7 默认对 0x20000000-0x3FFFFFFF 为 Non-cacheable，无需额外处理
  - 若将来启用 Cache 覆盖 AXI SRAM，需配置 MPU 子区域隔离

---

## 10. 启动流程

```
main()
  ├── MPU_Config()                          ← 0x30000000 non-cacheable
  ├── SCB_EnableICache/DCache               ← 指令+数据缓存启用
  ├── HAL_Init() + SystemClock_Config()     ← HSE 25MHz → 480MHz
  ├── MX_GPIO/DMA/DCMI/SPI4/ADC/TIM2_Init()
  ├── [USER CODE 2] DisplayService_Init()   ← OLED / LCD 初始化
  ├── [USER CODE 2] ADC 校准 + 状态文本
  ├── osKernelInitialize() + MX_FREERTOS_Init()
  └── osKernelStart()
        │
        ├── Init_Task    ─→ USB_DEVICE_Init → AppAdc_Init → "RTOS Ready" → 自删除
        ├── ADC1/2/3_Task ─→ 进入 DMA/轮询循环
        ├── Console_Task  ─→ 等待 USB CDC 输入
        └── SysMon_Task   ─→ 周期快照 + OLED 刷新
```

---

## 11. 数据流

```
ADC1 ──DMA──→ adc1_value[64] ──ISR 通知──→ ADC1_Task → 快照+平均 → g_adc1_avg
ADC2 ──DMA──→ adc2_value[64] ──ISR 通知──→ ADC2_Task → 快照+平均 → g_adc2_avg
ADC3 ──轮询──→ TEMPSENSOR / VREFINT ──→ g_temp_c_x10 / g_vdda_mv

USB CDC RX ──ISR──→ StreamBuffer ──→ Console_Task → CLI 解析 → 命令执行
                                                              ↓
                                      sysmon/adc 服务 ← 命令处理器
                                              ↓
                                        CLI 输出 → USBVcom_printf → USB CDC TX

DCMI ──DMA──→ CameraBuffer ──ISR──→ DCMI_FrameState=1
                                       ↓
                              camera hex/verify/pixels → USB CDC

SysMon_Task (200ms) → AppAdc_GetStatus → DisplayService → OLED 刷新
SysMon_Task (1s)   → 堆栈水位采样 → AppSystemSnapshot_t
```

---

## 12. 开发指南

### 12.1 添加新服务模块

1. 在 `Core/Inc/app.h` 中添加接口函数声明
2. 创建 `Core/Src/app_xxx.c` 实现
3. 创建兼容头 `Core/Inc/app_xxx.h`（内容仅 `#include "app.h"`）
4. 在 Keil 工程的 Application/User 组中添加 `app_xxx.c`
5. 如需 CLI 命令：在 `app_console.c` 中注册 `CLI_Command_Definition_t`

### 12.2 添加新 CubeMX 外设

1. 在 `.ioc` 中配置外设
2. 重新生成代码
3. 检查受影响文件的 `USER CODE` 区域是否完整（特别是 `main.c` / `freertos.c` / `stm32h7xx_it.c`）
4. 在 `stm32h7xx_hal_conf.h` 的 USER CODE 区取消 `#define HAL_XXX_MODULE_ENABLED` 注释（如生成时未自动启用）

### 12.3 CubeMX 重新生成后检查清单

| 文件 | 检查项 |
|------|--------|
| `main.c` | USER CODE BEGIN 2 的 `DisplayService_Init()` / ADC 校准 / MPU 代码是否完整 |
| `freertos.c` | USER CODE 的 `#include "app.h"` / `AppXxx_*()` 调用 / `HAL_ADC_ConvCpltCallback` 桥接 |
| `stm32h7xx_it.c` | USER CODE 的 `#include "dcmi.h"` |
| `stm32h7xx_hal_msp.c` | USER CODE 的 `ExitRun0Mode()` |
| `spi.h` | USER CODE 的 `LCD_Width`/`LCD_Height` 定义 |
| `dcmi.h` | USER CODE Includes 的 `#include "sccb.h"` / `#include "lcd_st7789.h"` |
| `main.c` | `MX_QUADSPI_Init()` / `MX_SDMMC1_SD_Init()` 是否保持注释状态 |
| `.sct` | `RW_AXI` / `RW_DMA` 区域是否正确（散列文件不因 CubeMX 重写） |

---

## 13. 已知问题

| # | 问题 | 影响 | 修复方向 |
|---|------|------|----------|
| 1 | `ADCData_QueueHandle` 创建但未使用 | 浪费 ~64B 堆 | 删除 CubeMX 中的队列配置 |
| 2 | OLED Init 中 `OLED_WR_Byte(0xAF)` 重复发送 | 无害 | 清理冗余 |
| 3 | `oled_spi_V0.2.c` 尾部有注释掉的重复 Init 代码 | 文件冗余 | 删除 |
| 4 | VSWR 与 HAL 时基共用 TIM6 | VSWR 启用时冲突 | VSWR 改用其他 TIM |
| 5 | 切换显示类型后坐标不兼容 | OLED/LCD 布局差异 | 定义坐标宏或逻辑坐标 |
| 6 | `ExitRun0Mode` 需手动添加空实现 | startup 调用但未定义 | CubeMX 应自动生成（H7 低功耗启用时） |
| 7 | DCMI CameraBuffer 未使用专用 non-cacheable 区域 | Cache 启用后可能数据不一致 | 配置 MPU 子区域隔离 |
