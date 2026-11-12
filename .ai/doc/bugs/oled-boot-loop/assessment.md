# 缺陷评估：固件超出应用分区导致黑屏

- **名称**：oled-boot-loop
- **创建日期**：2026-11-12
- **来源**：用户报告新程序烧录后屏幕不亮，并提供原理图。
- **结论**：confirmed
- **严重程度**：high
- **根因置信度**：high

## 实际与预期行为

- 实际：COM5 在八秒被动采集中反复输出 `rst:0x3 (RTC_SW_SYS_RST)`、`Saved PC:0x403cf94c`，OLED 不亮。
- 预期：通过启动镜像校验，执行 setup 并进入显示界面。
- 环境：ESP32-C3 revision v0.4，4MB Flash，USB Serial/JTAG；用户已确认断开加热电源，并授权调试及烧录。

## 复现与基线

- 使用 pyserial 在115200波特率被动读取COM5八秒，不发送命令。
- 使用 riscv32-esp-elf-addr2line 将保存地址解析到安装框架的 bootloader_qio_80m.elf：`bootloader_reset`，bootloader_utility.c:846。
- 使用 esptool 读回0x00000～0x10fff，保存在 `.pio/oled-boot-loop-boot-nvs-before.bin`。板上分区表与本地 partitions.bin 相同，应用首4096字节与本地 firmware.bin 相同。
- 两次全片读取分别在460800和115200下发生串口数据短包错误，均未形成备份文件；小范围启动区及NVS备份成功。
- JTAG连接因Windows USB驱动不可用失败；未更改驱动。

## 代码与运行证据

| 位置或命令 | 证据 | 判断 |
|---|---|---|
| 原理图及 include/Oled.h | SDA=GPIO4，SCL=GPIO5，128×32；软件一致 | 未发现显示接线配置冲突 |
| platformio.ini | 未指定分区表，采用default.csv | 每个OTA应用槽只有0x140000字节 |
| 板上分区读回 | app0=0x10000/0x140000；app1=0x150000/0x140000 | 确认实际分区容量1,310,720字节 |
| firmware.bin及esptool image_info | 镜像1,312,336字节，校验和与SHA256有效 | 完整镜像比单槽大1,616字节 |
| ROM日志及bootloader ELF | bootloader_reset地址0x403cf94c | 支持应用加载阶段拒绝镜像；不是OLED初始化后才重启 |
| 原理图 | OLED电源为3.3V；GPIO8/9参与启动，SW接GPIO9 | 实物电压和拉电阻不能仅由图片证实；本次日志为正常Flash启动模式 |

## 根因判断

应用镜像超过实际应用分区边界，bootloader无法正常加载，导致在setup之前循环复位，屏幕初始化没有执行。编译器统计的段占用不等于包含对齐空洞的最终烧录镜像长度，因此此前编译成功不能证明镜像适配分区。

## 修复范围与建议

- 仅修改platformio.ini：采用框架自带min_spiffs.csv，每个应用槽0x1e0000字节，保留双OTA；monitor_speed与Serial.begin统一为115200；删除无效且指向旧平台的upload选项，由PlatformIO自动识别或命令行指定端口。
- 保持NVS在0x9000、长度0x5000，保留参数与校准数据；不擦除全片。
- 不修改OLED引脚、驱动、I2C速度、PWM初始化及温控逻辑。
- 不新增永久测试：本次使用最终二进制与实际分区的容量检查，以及上板启动作为直接行为验证。

## 风险与验证要求

- min_spiffs.csv将第二OTA槽改为0x1f0000，SPIFFS改为0x3d0000/0x20000；旧文件系统布局不兼容。项目src/include没有使用文件系统接口。升级采用完整串口烧录分区表及应用，不可仅OTA切换分区表。
- 保留此前启动区/NVS备份，上传后比较原始NVS字节；运行时可能合法更新NVS，不能将字节不同直接认定损坏。
- 编译后检查最终.bin长度不超过所有OTA应用槽；烧录时观察哈希校验。
- 上板捕获日志确认不再循环复位；必须由用户观察真实OLED，不能用编译、日志或模拟器声称已经点亮。
- 暂未验证OLED地址、上拉电阻、供电、电气时序和实际画面；1MHz I2C为独立兼容风险，只有启动恢复后仍黑屏时才继续定位。

## 阶段检查

- [x] 实际与预期行为明确。
- [x] 根因包含具体证据和置信度。
- [x] 修复范围和验证要求可执行。
- [x] 用户已确认硬件隔离及上板调试授权；无阻塞修复问题。
