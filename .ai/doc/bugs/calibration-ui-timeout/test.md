# 缺陷验证报告：校准期间暂停同步联网

- **名称**：calibration-ui-timeout
- **验证日期**：2026-11-12
- **评估文件**：`./assessment.md`
- **修复文件**：`./fix.md`
- **结果**：partial

## 结论摘要

软件已按用户选择隔离活动校准与同步网络处理。主机在真实loop路径模拟1秒网络等待，P1仍持续调度并保持正常状态，本地取消后停热、冷却并恢复联网；真正心跳停更251ms的保护回归仍通过。生成的实际GRAM画面已打开检查，NET PAUSE及本地停止提示清楚可见。目标板不可达，未烧录或重跑实物P1过程，不能宣称原始实物UI TIMEOUT已消失。

## 验证记录

| 检查 | 命令或操作 | 状态 | 可观察结果 |
|---|---|---|---|
| 主机回归 | `OLED_PREVIEW_DIR=C:/Users/lance/proj/ESP32C3/.pio/host-oled-preview python test/host/run.py` | pass | 全部模型、OLED像素、ADC、持久化、稳定窗、PWM及UI检查通过，1.77秒 |
| P1同步网络阻塞隔离 | 同一主机测试的network_calibration_checks，实际main.cpp:loop | pass | 物联网开启，模拟网络单次等待1秒且每4ms执行监督；P1真实循环1000次后仍active、无停止原因、输出非零且无Flash提交 |
| 本地取消及恢复联网 | 同一新增用例，排入长按并执行loop及pwm_irq | pass | Cancelled锁存、输出归零、进入冷却、返回菜单；随后网络等待恢复执行，加热仍为零 |
| 真正主循环失联 | 既有pwm_checks中的251ms心跳停更用例 | pass | HeartbeatTimeout停热及全速风扇仍成立，没有放宽阈值 |
| 校准显示表面 | 打开`.pio/host-oled-preview/calibration-network-paused.png` | pass | P1点号与预算、温度、等待状态及HOLD:STOP / NET PAUSE四行均可见；这是生产软件帧缓冲，不是物理屏幕照片 |
| ESP32-C3固件构建 | `C:/Users/lance/.platformio/penv/Scripts/pio.exe run -e esp32-c3-devkitm-1` | pass | SUCCESS，9.83秒；RAM50,412字节，程序段1,252,650字节 |
| 最终镜像容量 | Python解析partitions.bin并比较firmware.bin实际长度 | pass | 镜像1,313,120字节，两个应用槽各1,966,080字节，均适配 |
| 目标板可达性 | PlatformIO Python执行`serial.tools.list_ports -v` | pass | 本次只有COM1，没有ESP32-C3串口 |
| 实物烧录与P1原始复现 | 无可达目标板，未执行 | not-run | 没有实物网络调用耗时证据，也没有修复后热平衡及完整标定结果 |
| 真实加热或温度精度验收 | 本次不执行 | skipped | 不自动启动加热；需独立限温、匹配供电与可靠参考测点 |

## 输出摘录

```text
PASS PWM: budgets, remote exclusion, stop precedence, all deadlines, rollover, cooling
PASS UI: full four-point flow, unstable input rejection, stop-before-save, queue-full cancel
PASS network calibration: real loop survives blocking network, local stop, networking resumes without reheating
All calibration behavior checks passed; electrical timing and heating remain unverified.
[SUCCESS] Took 9.83 seconds
firmware bytes: 1313120 OTA slots: [('app0', 1966080), ('app1', 1966080)]
PASS: complete image fits both application partitions
```

## 残余风险与操作约定

- 活动校准暂停所有物联网处理，远程关闭不可用；现场长按停止及独立断电保护仍是必要条件。用户已明确批准。
- 未调大250ms心跳，没有用后台心跳掩盖主循环失联；温度慢升仍需要等待真正热平衡，不改变1℃/2℃稳定范围或每点10分钟上限。
- 软件防止进入已知同步网络路径，不证明这次实物的具体阻塞调用已被抓获；若仍超时，应基于实际耗时继续定位，包括OLED总线错误等可能性，而不是继续放宽保护。
- 恢复联网后可能收到传输层缓存消息，冷却锁仍限制重新开启；不宣称清空缓存消息。
- 本次没有修改生产代码或测试来迁就验证结果；仅在通过烟测后更新需求、操作及缺陷记录。

## 建议

目标板重新连接后，在断开加热电源状态下烧录并检查暂停提示及本地退出；P1真实通电验证须由用户在独立限温及可靠参考测温条件下执行，并记录是否仍出现UI TIMEOUT。

## 阶段检查

- [x] 原始实物报告作为复现依据，修复后等价主机路径已执行。
- [x] 所有实际检查、未执行项目和限制均已记录。
- [x] 安全保护与本地取消未被削弱。
- [x] 实物未验证，结果为partial，不假称完成上板修复。
