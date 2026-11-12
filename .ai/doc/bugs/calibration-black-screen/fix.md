# 缺陷修复记录：补全真实OLED小字绘制

- **名称**：calibration-black-screen
- **修复日期**：2026-11-12
- **评估文件**：`./assessment.md`
- **状态**：applied

## 修复摘要

补全Oled驱动已声明但未实现的6×8字号，保留校准页四行布局及原有16像素文字。移除主机测试中的HostOled替身，校准流程改为执行生产OLED驱动，直接验证真实GRAM像素。

## 变更记录

| 文件 | 变更 | 对应根因 |
|---|---|---|
| include/oledfont.h | 增加95个可打印ASCII字符的固定6列字库 | 没有小字字形 |
| src/Oled.cpp | Size=8通过BMP写入6×8字形，str采用6像素步进及换行；限制字号与字库索引 | str拒绝8字号、write_char未实现小字且宽度未初始化 |
| test/host/support/Harness.h | 使用真实Oled.h，删除HostOled及重复方向常量 | 文本替身掩盖真实渲染失败 |
| test/host/support/Wire.h | 仅替代I2C传输边界，记录发送缓冲 | 主机不能直接访问硬件总线 |
| test/host/run.py | 编译真实Oled.cpp及像素回归测试 | 原测试未编译实际显示驱动 |
| test/host/oled_test.cpp | 独立字形像素、裁剪、擦除、换行、字符范围及16像素兼容检查；可选PBM输出 | 捕获空白页、字形错位及越界相关回归 |
| test/host/calibration_test.cpp | 确认、等待、录入、成功页面检查四行真实像素，保留完整四点及安全路径 | UI状态正常不代表页面可见 |

## 直接路径检查

- `OLED_PREVIEW_DIR=... python test/host/run.py`：通过。实际驱动的小字及既有模型、ADC、Flash、稳定窗、PWM安全边界、完整UI流程均通过。主机编译器为ziglang；输出仍明确未验证实物电气时序和加热。
- `C:/Users/lance/.platformio/penv/Scripts/pio.exe run -e esp32-c3-devkitm-1`：SUCCESS，20.80秒。RAM 50,412/327,680字节，程序段1,252,578/1,966,080字节。
- 使用真实UI及真实Oled.cpp产生确认、等待、录入、成功四张128×32 PBM，输出位于`.pio/host-oled-preview/`，用于后续像素画面检查，不是物理OLED照片。
- 既有中文字形代码的dangling-else及Blinker字符串告警仍存在，未扩大范围处理。

## 评估偏离

- **是否偏离**：否。
- **新证据**：主机测试使用真实OLED驱动后完整流程通过。
- **额外范围**：无。
- **处理决定**：验证生成画面与最终镜像大小；实物不可达时记录partial，不假称烧录完成。

## 已知风险

本次未改UI状态机或温控逻辑。初次软件检查时设备不可达；用户重连后已完成完整烧录、四区域哈希校验及实物确认：校准确认页四行正常显示，长按能退出。详见test.md。未启动真实校准加热，不证明温度精度或加热安全。

## 阶段检查

- [x] 修复对应确定的字号缺失根因。
- [x] 未修改无关功能。
- [x] 回归覆盖实际像素与完整校准行为。
- [x] 直接路径检查已运行并记录。
