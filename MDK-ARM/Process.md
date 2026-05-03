````markdown
# STM32H750 + FreeRTOS + USB CDC + 双路 ADC DMA + ADC3 温度采样：会话继承摘要

> 用途：当上下文不够时，把这份 Markdown 直接贴到下一次会话中，作为本项目的继承上下文。  
> 默认前提：**本次会话中讨论的优化项都已经完成并落地**；若实际代码与本文不一致，以当前工程代码为准并优先指出差异。

---

## 1. 项目背景

这是一个基于 **STM32H750 + FreeRTOS** 的工程，具备以下基础能力：

- **USB 虚拟串口（CDC）**，用于系统状态回传。原始工程已包含 `usb_device.c / usbd_cdc_if.c / usbd_conf.c / usbd_desc.c` 等 USB CDC 框架文件。:contentReference[oaicite:0]{index=0} :contentReference[oaicite:1]{index=1} :contentReference[oaicite:2]{index=2} :contentReference[oaicite:3]{index=3} :contentReference[oaicite:4]{index=4} :contentReference[oaicite:5]{index=5} :contentReference[oaicite:6]{index=6}
- **ADC1 / ADC2 使用 DMA 循环采样**，用于两路外部模拟量。原始配置中 ADC1/ADC2 都是 `ADC_CONVERSIONDATA_DMA_CIRCULAR`。:contentReference[oaicite:7]{index=7}
- **ADC3 不使用 DMA**，负责采集芯片内部参数；原始配置中 ADC3 是 `ADC_CONVERSIONDATA_DR`，regular channel 为 `ADC_CHANNEL_TEMPSENSOR`。:contentReference[oaicite:8]{index=8}
- **M7 打开 ICache / DCache**，并通过 MPU 将 `0x30000000` 起始的 32KB 区域设为 non-cacheable，用于 DMA 共享缓冲区。`main.c` 中配置了 MPU region1 为 `0x30000000 / 32KB / non-cacheable`，scatter 文件中为 `RW_DMA 0x30000000 0x00008000` 预留了 DMA 区。:contentReference[oaicite:9]{index=9} :contentReference[oaicite:10]{index=10}
- FreeRTOS 已开启 **run-time stats** 所需配置：`configGENERATE_RUN_TIME_STATS=1`、`configUSE_TRACE_FACILITY=1`、`configUSE_STATS_FORMATTING_FUNCTIONS=1`，并通过 `portCONFIGURE_TIMER_FOR_RUN_TIME_STATS` / `portGET_RUN_TIME_COUNTER_VALUE` 挂到 TIM2 计数器。:contentReference[oaicite:11]{index=11} :contentReference[oaicite:12]{index=12}
- TIM2 是 32 位自由运行定时器，适合作为运行时统计时间基。:contentReference[oaicite:13]{index=13}

---

## 2. 原始工程中确认过的关键点

### 2.1 DMA / Cache / MPU

- 原始 `main.c` 已经在启动时调用 `MPU_Config()`，然后开启 `SCB_EnableICache()` 和 `SCB_EnableDCache()`。:contentReference[oaicite:14]{index=14}
- 原始 scatter 文件把 `0x30000000 ~ 0x30007FFF` 作为 DMA 专用区，注释中也明确说明它与 `main.c` 里的 non-cacheable MPU 区对应。:contentReference[oaicite:15]{index=15}
- **原始问题**：`freertos.c` 中 DMA 缓冲变量使用的是 `section(".dma_buffer")`，而 scatter 文件匹配的是 `dma_buffer` / `dma_buffer.*`，名字不一致，可能导致变量没有真正落入 `RW_DMA`。本次会话中已明确要求修复为统一使用 `section("dma_buffer")`。:contentReference[oaicite:16]{index=16} :contentReference[oaicite:17]{index=17}

### 2.2 ADC / DMA / 中断

- 原始 ADC1 / ADC2 已分别挂接到 `DMA1_Stream0` / `DMA1_Stream1`，DMA 模式为 circular。:contentReference[oaicite:18]{index=18}
- `dma.c` 中 DMA1 Stream0/1 的中断优先级都配置为 **5**。:contentReference[oaicite:19]{index=19}
- `stm32h7xx_it.c` 中 DMA1_Stream0/1 IRQ 已正确调用 `HAL_DMA_IRQHandler(&hdma_adc1)` / `HAL_DMA_IRQHandler(&hdma_adc2)`。:contentReference[oaicite:20]{index=20}
- `FreeRTOSConfig.h` 中 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`，因此 DMA IRQ 使用 `5` 与 `vTaskNotifyGiveFromISR()` 等 FreeRTOS ISR-safe API 是匹配的。:contentReference[oaicite:21]{index=21}

### 2.3 USB CDC

- 原始 USB Device 使用 OTG FS，USB 时钟来自 HSI48；OTG_FS IRQ 优先级也配置为 **5**。:contentReference[oaicite:22]{index=22}
- 原始 `CDC_Transmit_FS()` 只判断 `TxState`，没有对 `hUsbDeviceFS.dev_state` 或 `pClassData == NULL` 做防御，也没有上层互斥保护。:contentReference[oaicite:23]{index=23}
- 原始 `USBVcom_printf()` 中 `vsnprintf()` 使用了 `sizeof(usbtemp) + 1`，存在长度处理不规范的问题。:contentReference[oaicite:24]{index=24}

### 2.4 原始 FreeRTOS 任务问题

- 原始 `ADC1_Handler()` / `ADC2_Handler()` 中直接对 DMA 原始数组 `adc1_value[0]` / `adc2_value[0]` 做累加平均，属于“边读边改 DMA 原始缓冲”的写法。:contentReference[oaicite:25]{index=25}
- 原始 `ADC1_Handler()` / `ADC2_Handler()` 还把 `LCD_DisplayNumber()` 放在 `taskENTER_CRITICAL()` 内，并配了 `osDelay(2000)`，会造成数据更新慢、临界区过长、实时性下降。:contentReference[oaicite:26]{index=26}
- 原始 `SysMon_Handler()` 是空循环，没有真正回传系统状态。:contentReference[oaicite:27]{index=27}

---

## 3. 本次会话最终约定并已完成的修改目标

> 下一次会话默认以这些“最终状态”为准继续工作。

### 3.1 DMA 缓冲区放置策略

- **DMA buffer 必须放在 `0x30000000` 的 non-cacheable 区域**，不走 cache 维护，避免 M7 D-Cache 与 DMA 数据不一致问题。
- `freertos.c` 中 DMA 缓冲区的 section 名称统一为：

```c
__attribute__((section("dma_buffer"), aligned(32)))
volatile uint16_t adc1_value[5];

__attribute__((section("dma_buffer"), aligned(32)))
volatile uint16_t adc2_value[5];
````

* `main.c` 中对 DMA 缓冲的 `extern` 声明改为 `volatile`，并在 `HAL_ADC_Start_DMA()` 返回值失败时进入 `Error_Handler()`。

**设计依据**：ST 官方建议 DMA 共享缓冲优先放 non-cacheable region；若 DMA 缓冲仍在 cacheable 区，则必须在 DMA 前后做 cache clean / invalidate。([STMicroelectronics][1])

---

### 3.2 ADC1 / ADC2 处理模型

* ADC1 / ADC2 继续使用 **DMA Circular**。
* 使用 `HAL_ADC_ConvCpltCallback()` + `vTaskNotifyGiveFromISR()` 唤醒对应任务。
* `ADC1_Handler()` / `ADC2_Handler()` 改成**事件驱动**：

  * `ulTaskNotifyTake(pdTRUE, portMAX_DELAY);`
  * 读取 DMA buffer 快照或直接只读平均
  * 更新 `g_adc1_avg` / `g_adc2_avg`
  * 增加 `ADC1_Cnt` / `ADC2_Cnt`
* **不再在 ADC 任务中调用 LCD**
* **不再在 ADC 任务中使用 `osDelay(2000)`**
* **不再修改 DMA 原始数组内容**

**设计依据**：FreeRTOS 文档说明 `ulTaskNotifyTake()` 可作为更轻量的任务同步机制，任务阻塞等待通知时**不占用 CPU**。([w3ww.freertos.org][2])

---

### 3.3 ADC3 处理模型

* ADC3 **不使用 DMA**，保持 `ADC_CONVERSIONDATA_DR` 模式。原始工程也是这样配置的。
* ADC3 任务周期性轮询采样以下内部通道：

  * `ADC_CHANNEL_TEMPSENSOR`
  * 可选：`ADC_CHANNEL_VREFINT`
* ADC3 任务负责更新：

  * `g_adc3_temp_raw`
  * `g_adc3_vref_raw`
  * `g_vdda_mv`
  * `g_temp_c_x10`
  * `ADC3_Cnt`
* 芯片温度与 VDDA 会在系统状态回传中一并输出。

---

### 3.4 SysMon 任务职责

`SysMon_Handler()` 已恢复并承担两类职责：

1. **低频 LCD 刷新**

   * 比如每 `200 ms` 更新一次显示
   * LCD 的慢操作放在 SysMon，而不是放在 ADC1/ADC2 任务里

2. **USB CDC 回传系统状态**

   * 约 `1 s` 一次
   * 回传内容至少包括：

     * `tick`
     * `heap_free`
     * `adc1_avg`
     * `adc2_avg`
     * `temp`
     * `vdda`
     * `ADC1_Cnt / ADC2_Cnt / ADC3_Cnt`
     * **各任务运行时统计信息**

---

### 3.5 FreeRTOS 运行时统计（必须保留）

本次会话明确要求：**系统状态回传时，不能只发温度等信息，必须同时发各任务统计信息**。

因此最终状态中：

* 保留 `configGENERATE_RUN_TIME_STATS=1`
* 保留 `configUSE_TRACE_FACILITY=1`
* 保留 `configUSE_STATS_FORMATTING_FUNCTIONS=1`
* 保留 `portCONFIGURE_TIMER_FOR_RUN_TIME_STATS` 和 `portGET_RUN_TIME_COUNTER_VALUE` 绑定 TIM2。
* `SysMon_Handler()` 中定期执行：

```c
memset(g_runtime_stats, 0, sizeof(g_runtime_stats));
vTaskGetRunTimeStats(g_runtime_stats);
```

* 通过 USB CDC 把 `g_runtime_stats` 和系统状态一起发回 PC。

**设计依据**：FreeRTOS 官方文档说明，启用 run-time stats 后，可通过 `vTaskGetRunTimeStats()` 以表格形式输出每个任务的运行时间统计；其时间基应高于 tick 中断频率，通常建议快 10～100 倍。([FreeRTOS][3])

---

### 3.6 LCD 访问策略

* **禁止**在 `taskENTER_CRITICAL()` 内做 LCD 显示。
* **禁止**在 ADC DMA 数据处理任务中直接调用 LCD。
* LCD 统一由低优先级或较低频率的任务更新，例如 SysMon。
* 需要互斥时使用 `LcdMutexHandle`，而不是长时间 critical section。

**结论**：
“ADC 任务里没有 `osDelay()` 会不会卡死？”——**不会**，因为任务阻塞在 `ulTaskNotifyTake()` 时不占 CPU；真正要避免的是在临界区里做慢 LCD 操作。([w3ww.freertos.org][2])

---

### 3.7 USB CDC 发送路径

最终状态应具备这些增强：

* 在 `usbd_cdc_if.h` 中新增：

```c
uint8_t CDC_IsReady_FS(void);
```

* 在 `usbd_cdc_if.c` 中新增 `CDC_IsReady_FS()`：

  * 仅当 `hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED`
  * 且 `hUsbDeviceFS.pClassData != NULL`
  * 才返回 ready

* `CDC_Transmit_FS()` 中增加：

  * `Buf == NULL` / `Len == 0` 防御
  * `pClassData == NULL` 防御
  * `TxState != 0` 时返回 `USBD_BUSY`

* `USBVcom_printf()` 中：

  * 修正 `vsnprintf()` 长度
  * 增加 `UsbTxMutexHandle`
  * 对 `USBD_BUSY` 做短时间重试
  * 在 USB 未 ready 时直接放弃当前发送而不是崩溃

**设计依据**：ST 的 USB Device CDC 流程基于 `USBD_CDC_SetTxBuffer()` 与 `USBD_CDC_TransmitPacket()`，上层应处理设备是否已配置、端点是否 busy 等状态。([STMicroelectronics][4])

---

## 4. 代码风格与移植约束

### 4.1 必须遵守 `USER CODE BEGIN / END`

后续继续修改时，默认遵守以下规则：

* 优先只在 `/* USER CODE BEGIN ... */` 和 `/* USER CODE END ... */` 之间写自定义代码
* 尽量不直接改 CubeMX 生成区
* 如必须改动生成区附近，只做最小侵入调整，并说明原因
* 便于后续重新生成代码时保留用户逻辑

### 4.2 任务职责边界

* **Init_Task**：初始化 USB 设备并自删
* **ADC1_Task / ADC2_Task**：只处理 ADC DMA 数据，不做 LCD 慢操作
* **ADC3_Task**：轮询内部温度 / VREFINT
* **SysMon_Task**：统一做 LCD 低频刷新和 USB 状态回传
* **Console_Task**：预留给 CDC 接收命令解析

---

## 5. 本次会话中明确修过或强调过的具体问题

1. **修复 DMA section 名称不一致**

   * 原始：`.dma_buffer`
   * 目标：`dma_buffer`
   * 目的：确保落入 scatter 中的 `RW_DMA` 区。 

2. **修复 USBVcom_printf 长度处理**

   * 原始 `vsnprintf((char*)usbtemp, sizeof(usbtemp) + 1, ...)`
   * 改为使用 `sizeof(usbtemp)`，并在超长时截断。

3. **恢复系统统计回传**

   * 不能只发温度和 ADC 数据
   * 必须一并发 `vTaskGetRunTimeStats()` 的输出

4. **去掉 ADC1/ADC2 任务中的 `osDelay(2000)`**

   * 改为纯事件驱动
   * 防止采样与显示严重滞后

5. **LCD 慢操作移出 critical section**

   * 避免长时间禁止抢占或影响中断响应

---

## 6. 当前系统的推荐回传格式

推荐 USB CDC 每秒回传一帧，格式类似：

```text
[SYS] tick=123456 heap=9876 adc1_avg=12345 adc2_avg=23456 temp=36.7C vdda=3298mV cnt1=1000 cnt2=1000 cnt3=10
---------------- Runtime Stats ----------------
TaskName        Abs Time      % Time
Init_Task       ...
SysMon_Task     ...
ADC1_Task       ...
ADC2_Task       ...
ADC3_Task       ...
Console_Task    ...
IDLE            ...
Tmr Svc         ...
-----------------------------------------------
```

---

## 7. 后续继续优化时的优先级建议

如果下次继续做，建议优先级如下：

### P1：验证项

* 检查 map 文件，确认 `adc1_value` / `adc2_value` 的最终地址确实落在 `0x30000000` 区间
* 检查 `ADC1_Cnt / ADC2_Cnt / ADC3_Cnt` 是否持续增长
* 检查温度与 VDDA 是否合理
* 检查 USB CDC 在枚举前、枚举后是否稳定
* 检查 SysMon 栈是否足够，必要时继续加大

### P2：功能补全

* 增加 CDC 接收环形缓冲
* 在 `Console_Task` 中加入串口命令解析
* 支持查询当前 ADC 原始值、平均值、温度、任务负载
* 根据需要把 `VREFINT`、`VBAT` 等内部参数进一步纳入状态上报

### P3：工程化增强

* 为 LCD / USB / ADC 共享状态增加更明确的封装
* 给关键任务增加 stack watermark 诊断
* 对 USB 发送加入队列化而不是忙等重试
* 结合实际 APB timer clock，把 run-time stats 的时间单位解释得更直观

---

## 8. 如果下一次会话要继续，请直接从这里开始

下一次会话可直接使用下面这段提示语：

> 这是 STM32H750 + FreeRTOS 项目的续聊。
> 当前工程已经完成以下改造：
>
> 1. ADC1/ADC2 使用 DMA circular，DMA buffer 放在 `0x30000000` non-cacheable 区，section 名称统一为 `dma_buffer`；
> 2. ADC1/ADC2 通过 `HAL_ADC_ConvCpltCallback + vTaskNotifyGiveFromISR + ulTaskNotifyTake` 事件驱动处理；
> 3. ADC3 不使用 DMA，负责 TEMPSENSOR / VREFINT 周期采样，并计算芯片温度与 VDDA；
> 4. SysMon 任务负责 LCD 低频刷新与 USB CDC 系统状态回传；
> 5. 系统状态回传必须同时包含芯片温度、VDDA、ADC 平均值、计数器以及 `vTaskGetRunTimeStats()` 输出；
> 6. USB CDC 已补充 ready / busy / mutex 防护；
> 7. 所有后续修改优先遵守 `USER CODE BEGIN / END` 规范。
>    请在此基础上继续优化，不要回退这些设计。

---

## 9. 这次会话用到并确认过的重要文件

* `adc.c / adc.h`：确认 ADC1/2 为 DMA circular，ADC3 为非 DMA 温度采样通道。 
* `dma.c`：确认 DMA IRQ 优先级为 5。
* `stm32h7xx_it.c`：确认 DMA 和 USB OTG FS IRQ 已接入 HAL 入口。
* `main.c`：确认 MPU / cache / ADC 校准 / ADC1/2 启动 DMA。
* `Project_sct.txt`：确认 `RW_DMA 0x30000000 0x00008000` 的 scatter 区域。
* `FreeRTOSConfig.h`：确认 run-time stats 宏与中断优先级边界。
* `freertos.c`：确认原始任务框架、原始问题点以及本次修改落点。
* `tim.c`：确认 TIM2 为运行时统计计数基准。
* `usb_device.c / usbd_conf.c / usbd_desc.c / usbd_cdc_if.c / usbd_cdc_if.h`：确认 USB CDC 初始化与发送路径。    

---

## 10. 核心原则（简版）

* **DMA buffer 不和 D-Cache 打架**
* **ADC 快路径只做快事**
* **LCD / USB 这类慢路径放到低频任务**
* **系统状态回传必须带任务统计**
* **优先写在 USER CODE BEGIN/END 里**
* **后续优化不要回退这套职责分层**

```

如果你愿意，我下一条可以把这份继承文档再压缩成“更适合贴进新会话首条消息”的短版。
::contentReference[oaicite:54]{index=54}
```

[1]: https://www.st.com/content/ccc/resource/technical/document/application_note/group0/08/dd/25/9c/4d/83/43/12/DM00272913/files/DM00272913.pdf/jcr%3Acontent/translations/en.DM00272913.pdf?utm_source=chatgpt.com "Level 1 cache on STM32F7 Series and STM32H7 Series"
[2]: https://w3ww.freertos.org/zh-cn-cmn-s/Documentation/02-Kernel/04-API-references/05-Direct-to-task-notifications/03-ulTaskNotifyTake?utm_source=chatgpt.com "ulTaskNotifyTake, ulTaskNotifyTakeIndexed - FreeRTOS™"
[3]: https://shkp.freertos.org/zh-cn-cmn-s/Documentation/02-Kernel/02-Kernel-features/08-Run-time-statistics?utm_source=chatgpt.com "运行时间统计 - FreeRTOS™"
[4]: https://www.st.com/resource/zh/user_manual/um1734-stm32cube-usb-device-library-stmicroelectronics.pdf?utm_source=chatgpt.com "STM32Cube USB"
