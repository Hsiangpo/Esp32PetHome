# Esp32PetHome

智能宠物屋课设项目，采用设备侧（ESP32）+ 应用侧（HarmonyOS App）+ 华为云 IoTDA 的端云协同架构。

## 项目结构

- `PetHome_Device`：ESP32 设备侧固件（PlatformIO + Arduino）
- `PetHome_APP`：鸿蒙应用侧（API 17）
- `docs`：PRD、接口协议、云端参数、接线说明、验收清单、技术文档与计划
- `scripts`：代码门禁脚本

## 快速开始

### 设备侧（ESP32）

1. 安装 VSCode + PlatformIO IDE 扩展。
2. 打开 `PetHome_Device` 目录。
3. 编译：
   ```bash
   python -m platformio run -d PetHome_Device -e esp32s3
   ```
4. 烧录到开发板后，按 `docs/Wiring_Guide.MD` 完成接线与上电测试。

### 应用侧（HarmonyOS）

1. 使用 DevEco Studio 打开 `PetHome_APP`。
2. 启动 HarmonyOS 模拟器或真机。
3. 运行 `entry` 模块，进入 App 首页后执行设备绑定与联调。

## 关键文档

- 需求文档：`docs/PRD.MD`
- 接口协议：`docs/Interface.MD`
- 云端参数：`docs/HuaweiCloud.MD`
- 技术口径：`docs/TECH.MD`
- 接线指南：`docs/Wiring_Guide.MD`
- 验收清单：`docs/Acceptance_Checklist.MD`

## 注意事项

- 本项目按课设口径进行演示实现，云端参数采用明文硬编码方式管理（详见 `docs/HuaweiCloud.MD`）。
- 提交代码前建议执行：
  ```bash
  python scripts/code_guard.py
  ```
