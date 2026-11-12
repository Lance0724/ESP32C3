# 缺陷验证报告：温度校准小字恢复

- **名称**：calibration-black-screen
- **验证日期**：2026-11-12
- **评估文件**：`./assessment.md`
- **修复文件**：`./fix.md`
- **结果**：verified

## 结论摘要

已修复确定的字号缺失问题。真实UI状态机调用真实Oled.cpp后，确认、等待、录入及成功页面均生成完整四行像素。随后用户重新连接目标板，本机完成完整烧录及哈希校验；用户实物确认校准确认页四行正常显示，长按也能退出。原始进入校准后黑屏的症状已验证消失；不将结果扩展为真实加热或温度精度验收。

## 验证记录

| 检查 | 命令或操作 | 状态 | 可观察结果 |
|---|---|---|---|
| 原始进入路径的主机等价检查 | `OLED_PREVIEW_DIR=C:/Users/lance/proj/ESP32C3/.pio/host-oled-preview python test/host/run.py` | pass | 长按进入CONFIRM即生成四行真实GRAM像素，进入时加热输出仍为零 |
| 实际OLED驱动像素回归 | 同一命令编译src/Oled.cpp及oled_test.cpp | pass | 独立A字形像素、跨页与四边裁剪、裁剪窗口、擦除、自动换行/不换行、字符范围及16像素兼容均通过 |
| 既有校准流程与安全回归 | 同一主机测试运行 | pass | 模型、ADC、持久化、稳定窗口、PWM边界、四点录入、失稳拒绝、停热后保存及队列满取消全部通过 |
| 实际像素图检查 | 打开`.pio/host-oled-preview/calibration-preview.png` | pass | 四张128×32缓冲区预览：确认、等待、录入、成功，均清晰可见，文字处于对应8像素行 |
| ESP32-C3构建 | `C:/Users/lance/.platformio/penv/Scripts/pio.exe run -e esp32-c3-devkitm-1` | pass | SUCCESS，20.80秒；RAM50,412字节，程序段1,252,578字节 |
| 完整镜像容量 | Python用struct解析生成分区表并逐应用槽检查firmware.bin长度 | pass | 完整镜像1,313,024字节，两个应用槽各1,966,080字节，全部适配 |
| 设备可达性 | PlatformIO Python执行`serial.tools.list_ports -v` | pass | 用户重连后识别COM5，VID:PID=303A:1001，序列号与先前目标板一致 |
| 完整烧录 | esptool 4.11.0，ESP32-C3，COM5，460800波特率，write_flash -z，模式/频率/容量均keep；写入0x0启动程序、0x8000分区表、0xe000 OTA启动数据、0x10000应用 | pass | 应用1,313,024字节；四个区域均Hash of data verified，执行硬复位；未覆盖NVS |
| 启动观察 | pyserial在COM5以115200波特率、DTR/RTS为False被动读取十秒 | pass | 0字节、0条ESP-ROM启动日志；结合实物观察确认，不单凭无日志证明正常运行 |
| 原始菜单进入及退出 | 用户长按进入温度校准确认页并长按退出 | pass | 用户选择screen=normal，即“四行正常显示，长按也能退出” |
| 真实加热及温度精度 | 本次不执行 | skipped | 未启动任何实物校准或加热任务 |

## 输出摘录

```text
PASS OLED: real 6x8 glyph pixels, clipping, erase, wrap, ASCII range, legacy 8x16
PASS UI: full four-point flow, unstable input rejection, stop-before-save, queue-full cancel
All calibration behavior checks passed; electrical timing and heating remain unverified.
[SUCCESS] Took 20.80 seconds
firmware bytes: 1313024 OTA slots: [('app0', 1966080), ('app1', 1966080)]
PASS: final image fits every application slot
Wrote 1313024 bytes (773863 compressed) at 0x00010000 in 8.8 seconds
Hash of data verified.
Observed seconds: 10
Bytes received: 0
ROM boot banners: 0
```

## 残余风险与缺失证据

- 确认页进入、四行显示与长按退出已完成实物验证；等待、录入和成功页面的像素已有主机检查，但尚未通过真实加热跑完四点标定。
- 本次按断开加热电源、仅USB供电的验证步骤操作，未发送任何加热命令；提示用户只观察确认页，不单击启动校准。
- 编译中原有Blinker字符串告警及主机编译暴露的既有中文字形dangling-else警告未扩展处理。
- 本次未改温控逻辑，也未验证真实加热安全或测温精度。

## 建议

关闭本次校准菜单黑屏缺陷。真实加热、完整实物四点标定及温度精度仍需独立安全条件与参考测温验证，不由本次显示修复代替。

## 阶段检查

- [x] 已执行真实菜单进入及退出路径，用户确认原始黑屏症状消失。
- [x] 所有检查均记录真实状态。
- [x] 验证阶段未修改生产源代码或测试来通过检查。
- [x] 未将软件像素证明或编译成功冒充实物修复完成。
