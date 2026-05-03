# STM32H7 FreeRTOS CubeMX 快速开发实战手册

> 基于 STM32H750VBT6 FreeRTOS 模板项目 (v2.2.1) 全流程经验总结。
> 适用于所有 STM32H7 + CubeMX + Keil + FreeRTOS 项目。
> 新会话继承上下文时，可直接贴入此文件。

---

## 一、核心设计哲学

### 1.1 CubeMX 是你的敌人，也是你的朋友

CubeMX 是代码**生成器**，不是代码**管理器**。它只负责 HAL 外设初始化，不负责业务逻辑。

**铁律：CubeMX 生成区只读，USER CODE 区可写。**

```
/* USER CODE BEGIN xxx */     ← 手写代码存活区
  你的代码
/* USER CODE END xxx */       ← 重新生成时以外的代码全部消失
```

每次重新生成前先 `git commit`，生成后用 `git diff` 检查谁被覆盖了。

### 1.2 高内聚低耦合的落地方式

不是口号，是文件结构：

```
app.h          ← 唯一合法的跨模块通信通道
app_xxx.h      ← 兼容包装头，内容仅一行 #include "app.h"
app_xxx.c      ← 实现，只包含自己的 .h + 真正需要的外设头文件
freertos.c     ← 薄薄一层：创建 RTOS 对象 + Handler 转发 + 回调桥接
```

**任何 app_foo.c 直接 #include "app_bar.h" 都是架构违规。**

### 1.3 为什么需要这个设计

CubeMX 重新生成时会覆盖 `main.c`、`freertos.c` 等文件的非 USER CODE 区域。如果你在这些文件中写了大量业务代码，每次生成就是一场灾难。

正确做法：CubeMX 文件只保留**桥接代码**（Handler 转发、callback 转发），真正的逻辑全部外移到 `app_*.c`。

---

## 二、文件体系速查

### 2.1 必知必会的文件角色

| 文件 | 角色 | CubeMX? | 修改自由度 |
|------|------|---------|-----------|
| `app.h` | 跨模块 API + 类型 + RTOS 对象导出 | ❌ | 100% |
| `freertos.c` | 任务/队列/互斥锁创建 + Handler/Callback 转发 | ✅ | USER CODE 区 |
| `main.c` | 时钟/MPU/外设初始化 + 启动调度器 | ✅ | USER CODE 区 |
| `stm32h7xx_it.c` | 中断服务函数 | ✅ | USER CODE 区 |
| `*.c` (CubeMX 外设) | HAL 外设初始化 (MX_XXX_Init) | ✅ | USER CODE 区 |
| `FreeRTOSConfig.h` | FreeRTOS 内核配置 | ✅ | USER CODE 区 |

### 2.2 USER CODE 区域手写内容速查

**main.c 需要放的东西：**
- `USER CODE BEGIN Includes`: 引入 `app.h`、`display_service.h` 等
- `USER CODE BEGIN PV`: 外部变量声明
- `USER CODE BEGIN 2`: 显示初始化、ADC 校准、启动状态文本

**freertos.c 需要放的东西：**
- `USER CODE BEGIN Includes`: `#include "app.h"`
- 每个 Handler 函数内: `AppXxx_Task(argument)`
- `USER CODE BEGIN Application`: `HAL_ADC_ConvCpltCallback` 桥接到 `AppAdc_DmaConvCpltCallback`

**stm32h7xx_it.c 需要放的东西：**
- `USER CODE BEGIN Includes`: 引入外设头文件（如 `dcmi.h`）

**stm32h7xx_hal_msp.c 需要放的东西：**
- `USER CODE BEGIN 1`: `ExitRun0Mode()` 空实现（startup 无条件调用）

---

## 三、新模块开发标准流程

### 3.1 纯软件模块（无新外设）

**步骤：**
```
1. app.h → 添加你的 API 声明
2. Core/Inc/app_xxx.h → 创建兼容包装头
3. Core/Src/app_xxx.c → 写实现
4. app_console.c → 注册 CLI 命令（可选）
5. 打开 Keil → 右键 Application/User 组 → Add Existing Files → 选 app_xxx.c
6. 编译 → 烧录 → 测试
```

**app_xxx.h 模板：**
```c
#ifndef APP_XXX_H
#define APP_XXX_H
#include "app.h"
#endif
```

### 3.2 新外设模块（需 CubeMX 配置）

**步骤：**
```
1. git commit（备份当前状态）
2. .ioc 中配置外设 → NVIC 使能 + 优先级 5
3. 重新生成代码
4. 检查 main.c: MX_XXX_Init() 是否被取消注释
5. git diff 检查是否有意外覆盖
6. 创建 app_xxx.c/h（同 3.1）
7. 如果外设初始化可能长时间阻塞（SDMMC/USB），用异步任务包装
```

### 3.3 第三方库集成（FatFs / LittleFS 等）

**核心原则：CubeMX 中间件保持 disable，手动管理所有文件。**

**步骤：**
```
1. 第三方源码 → Core/Src 和 Core/Inc
2. 创建适配层（bsp_xxx.c, lfs_port.c）连接 HAL 和库
3. 创建 app_xxx.c 应用层封装
4. 在 .uvprojx 新建 Group，添加文件
5. .ioc 中该中间件保持 disable（避免 CubeMX 生成冲突的 RTE 文件组）
6. 如果 CubeMX 重新生成后自动添加了无效的 RTE 组 → Keil 中直接删除
```

---

## 四、DMA + Cache 一致性 — 血泪教训

### 4.1 问题本质

Cortex-M7 有 D-Cache。DMA 直接写物理 RAM，不走 Cache。CPU 读同一地址时，如果 Cache 中有旧数据，读到的是**过期数据**。

### 4.2 解决方案对比

| 方案 | 做法 | 适用场景 | 风险 |
|------|------|----------|------|
| **Non-Cacheable MPU 区域** | sct + MPU + section attribute 三者配合 | 优先选择 | 需正确对齐，否则覆盖其他数据导致崩溃 |
| **SCB_InvalidateDCache** | 每次读 DMA 缓冲区前调用 | 临时方案 | 可能引发 HardFault 或系统不稳定 |

### 4.3 正确实现 Non-Cacheable DMA 区域

**三步缺一不可：**

**① `.sct` 散列文件：**
```
RW_DMA 0x30000000 0x00008000 {
  *(.bss.dma_buffer)
}
```

**② `main.c` MPU 配置（在 `.ioc` 中配）：**
```
CORTEX_M7 → MPU Region 1:
  BaseAddress = 0x30000000
  Size        = Size32KBytes
  Cacheable   = Disabled
  Bufferable  = Disabled
  Shareable   = Enabled
```

**③ 变量定义：**
```c
__attribute__((section(".bss.dma_buffer"), aligned(32)))
volatile uint16_t buffer[SIZE];
```

### 4.4 我们踩过的坑

**坑1：裸地址 O(0x24000000) 与 scatter `.ANY` 区域重叠**

DMA 直接写 `0x24000000`，但 scatter 中 `RW_AXI 0x24000000 0x00080000 { .ANY (+RW +ZI) }` 把 FreeRTOS 堆和全局变量也放在此区域。DMA 写摄像头数据时，直接覆盖了 FreeRTOS 堆中的任务控制块 → 系统瞬间崩溃。

**教训：DMA 缓冲区要么用 section attribute 放在 scatter 显式区域，要么用裸地址但必须确认该地址不在 scatter 的 `.ANY` 范围内。**

**坑2：ARMCC ZI 变量 section 命名自动加 `.bss.` 前缀**

代码中写 `__attribute__((section(".camera_buffer")))` 但链接器期望匹配 `*(.bss.camera_buffer)`。散列文件写 `*(.camera_buffer)` 匹配不上 → 变量回退到 `.ANY` 分配 → 位置不可控。

**教训：使用 `*(.bss.xxx)` 或 `*(.xxx*)` 通配符匹配。**

**坑3：MPU 子区域和散列分区的组合爆炸**

修改散列分区大小后忘了同步更新 MPU → 系统崩溃。AXI SRAM 从 512KB 分成 256KB+256KB 后，FreeRTOS 堆空间不够 → 静默失败（无 HardFault，直接卡死）。

**教训：sct 分区和 MPU 配置必须联动更新，改一个必须检查另一个。**

---

## 五、阻塞操作处理模式

### 5.1 问题

`HAL_SD_Init()` 在无 SD 卡时不返回，阻塞调用它的 FreeRTOS 任务。如果在 Console_Task 中调用，整个 CLI 停止响应。

### 5.2 解决：异步任务模式

```c
static volatile bool g_xxx_pending = false;

void AppXxx_MountAsync(void)
{
  if (g_xxx_mounted || g_xxx_pending) return;
  g_xxx_pending = true;
  xTaskCreate(AppXxx_MountTask, "XxxMnt", 1024, NULL,
              tskIDLE_PRIORITY + 3, NULL);
}

static void AppXxx_MountTask(void *arg)
{
  (void)arg;
  USBVcom_printf("[Xxx] Working, please wait...\r\n");

  // 这里放可能阻塞的初始化代码
  bool ok = HAL_Xxx_Init(...);

  g_xxx_mounted = ok;
  USBVcom_printf("[Xxx] %s\r\n", ok ? "OK" : "FAILED");
  g_xxx_pending = false;
  vTaskDelete(NULL);
}
```

**CLI 命令对应用法：**
```c
if (subcmd == "mount")
{
  if (AppXxx_IsMounted())
    snprintf(buf, len, "Already mounted.\r\n");
  else
  {
    AppXxx_MountAsync();
    snprintf(buf, len, "Mount started. Wait for [Xxx] message...\r\n");
  }
}
```

---

## 六、CLI 命令标准注册模板

### 6.1 四步法

```c
// 步骤1：前向声明 handler
static BaseType_t CLI_FooCmd(char *buf, size_t len, const char *cmd);

// 步骤2：命令定义
static const CLI_Command_Definition_t xFooCmd = {
  "foo",
  "foo: Foo description\r\n"
  "  foo bar  - Do something\r\n"
  "  foo baz  - Do something else\r\n",
  CLI_FooCmd,
  -1  // -1 = variable args; 0 = no args
};

// 步骤3：注册（在 AppConsole_RegisterCommands 中）
FreeRTOS_CLIRegisterCommand(&xFooCmd);

// 步骤4：实现 handler
static BaseType_t CLI_FooCmd(char *buf, size_t len, const char *cmd)
{
  const char *sub = FreeRTOS_CLIGetParameter(cmd, 1, &sub_len);
  if (!sub) { snprintf(buf, len, "Missing subcommand\r\n"); return pdFALSE; }

  if (strncmp(sub, "bar", 3) == 0) { /* ... */ }
  else if (strncmp(sub, "baz", 3) == 0) { /* ... */ }
  else { snprintf(buf, len, "Unknown: %.*s\r\n", sub_len, sub); }
  return pdFALSE;  // pdTRUE 表示还有后续数据（大数据量分段输出）
}
```

### 6.2 大数据量分段输出模式

当输出超过 `CONSOLE_OUTPUT_MAX_LEN` (768 bytes) 时：
- 首次调用：将全部数据写入静态 buffer，`s_offset = 0`，返回 `pdTRUE`
- 后续调用：从 `s_offset` 继续输出，直到全部发完，返回 `pdFALSE`

---

## 七、编译错误速查表

| 错误码 | 原因 | 修复 |
|--------|------|------|
| `#20: identifier "bool" is undefined` | C 环境缺 `<stdbool.h>` | 头文件中 `#include <stdbool.h>` |
| `#20: identifier "LCD_Width" is undefined` | 调用链缺少 `spi.h` 或 `lcd_st7789.h` | 检查 USER CODE Includes |
| `L6218E: Undefined symbol AppXxx_Func` | `.c` 文件未编译 | 在 `.uvprojx` 对应 Group 添加文件 |
| `L6218E: Undefined symbol MX_FATFS_Init` | CubeMX 生成了调用但没有实现 | 提供 `void MX_FATFS_Init(void)` 空函数 |
| `L6218E: Undefined symbol ExitRun0Mode` | startup 调用但 HAL 未定义（无低功耗配置时） | 在 `hal_msp.c` USER CODE 添加空函数 |
| `#223-D: function "xxx" declared implicitly` | 缺少头文件或函数未声明 | 添加对应 `#include` 或前向声明 |
| `#1035-D: single-precision operand implicitly converted to double` | 浮点常量默认 double | 添加 `f` 后缀：`3.14f` |
| `#159-D: declaration incompatible with previous` | 函数使用前未声明 | 添加前向声明或移动函数位置 |
| 重新生成后编译失败 | CubeMX 添加了不存在的 RTE 文件组 | Keil 中删除新增的无效 Group |
| system hang after init | `MX_SDMMC1_SD_Init()` 或其他阻塞调用在 main 中 | 保持注释，通过异步 CLI 命令初始化 |

---

## 八、硬件踩坑录

### 8.1 QSPI W25Q64

- CubeMX 默认 `ClockPrescaler=1` → 120MHz，**超过 W25Q64 上限 104MHz**，必须改为 2
- 1-line 模式稳定，4-line QPI 模式需要额外配置状态寄存器使能
- 擦除操作后必须 `WaitBusy` 轮询状态寄存器 bit0

### 8.2 SDMMC1

- `HAL_SD_Init()` 无卡时**不是返回错误而是死等**，必须用异步任务包装
- `SD_read/write` 中的 `while(BSP_SD_GetCardState)` 无超时，需手动加超时计数
- FatFs `DISABLE_SD_INIT` 宏定义后 `SD_initialize` 跳过硬件初始化，需外部手动调用

### 8.3 OV2640 + DCMI

- SCCB (GPIO 模拟 I2C) 需要先配置 GPIO 才能读 ID
- DCMI DMA2_Stream7 中断优先级与 FreeRTOS `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 必须匹配
- Camera Buffer 必须放在 non-cacheable 区域，否则读取时看到的是过期的 Cache 数据

### 8.4 OLED SSD1306 + LCD ST7789

- 两者共用 SPI4，时钟速度不同：OLED 需 3.75MHz，LCD 需 60MHz
- `display_service.c` 通过编译开关 `APP_DISPLAY_TYPE` 选择驱动
- OLED Y 坐标是页寻址（y/8），LCD 是像素寻址

---

## 九、Git 工作流建议

```
每次重新生成 CubeMX 代码前：
1. git add -A && git commit -m "pre-regen"

重新生成后：
2. git diff 检查变化
3. 如有意外覆盖，手动修复
4. git add -A && git commit -m "post-regen fix xxx"

推送前：
5. git log --oneline -5 确认提交历史清晰
6. git push origin main
```

---

## 十、一句话总结

| 原则 | 一句话 |
|------|--------|
| 代码位置 | 业务逻辑进 `app_*.c`，CubeMX 只留桥接代码 |
| 跨模块通信 | 只通过 `app.h`，禁止 `app_xxx.h` 横向依赖 |
| 外设配置 | 所有硬件参数在 `.ioc` 中配置，不在代码里直接写寄存器 |
| 阻塞操作 | 用 `xTaskCreate` 异步任务包装，不阻塞 CLI 线程 |
| DMA 内存 | sct + MPU + section attribute 三件套，缺一不可 |
| 重新生成 | 先 commit，后 diff，再修复 |
| 第三方库 | CubeMX 中间件 disable，手动管理工程文件 |
