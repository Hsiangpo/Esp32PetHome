# 技术实现文档（TECH）

本文档描述 Esp32PetHome 的当前技术口径。若与 `docs/PRD.MD` 冲突，以 PRD 为准并同步修订。

---

## 1. 三端架构

本项目采用 PetHome 三端直连模式：

- 设备端：ESP32 固件，通过 MQTT 对接华为云 IoTDA
- App 端：HarmonyOS 应用，**直接调用 IoTDA App Endpoint REST API**
- 云端：华为云 IoTDA（设备模型、影子、命令、在线状态）

说明：

- App 默认不依赖本地自建后端即可完成影子查询与命令下发。
- 历史/日报在当前版本由 App 本地聚合演示数据生成，用于课堂演示闭环。

---

## 2. 开发环境

### 2.1 固件端（ESP32）

- VSCode + PlatformIO IDE
- 命令：

```bash
python -m platformio run -d PetHome_Device -e esp32s3
python -m platformio run -d PetHome_Device -e esp32s3 -t upload
python -m platformio device monitor -d PetHome_Device
```

### 2.2 App 端（HarmonyOS）

- 工程：`PetHome_APP/`
- 入口页：`entry/src/main/ets/pages/Index.ets`
- 云配置：`entry/src/main/ets/config/iotdaDefaults.ets`
- REST 客户端：`entry/src/main/ets/service/PetHomeApi.ets`
- AK/SK 签名：`entry/src/main/ets/api/iotdaSigner.ets`

---

## 3. 固件端实现

核心文件：`PetHome_Device/src/main.cpp`

### 3.1 功能模块

- `sensor_manager.*`：传感器采样
- `actuator_manager.*`：执行器控制与互锁
- `control_rules.*`：自动策略判断
- `cloud_client.*`：Wi-Fi + MQTT + 遥测/事件/回执
- `ap_provision_manager.*`：AP 配网（`/status` `/scan` `/config`）
- `config_store.*`：NVS 参数持久化

### 3.2 已实现关键策略

- Wi-Fi 失败重试与 AP 兜底
- 自动投喂、自动补水、自动排风、自动照明
- 互锁保护（泵与舵机不并发）
- 事件上报：`LOW_FOOD`、`LOW_WATER`、`WATER_REFILL_FAILED`、`FEED_FAILED`、`WIFI_OFFLINE`、`CLOUD_OFFLINE`

---

## 4. App 端实现

### 4.1 连接口径

App 按 IoTDA 原生 API 直连，使用 AK/SK `SDK-HMAC-SHA256` 签名：

- `GET /v5/iot/{project_id}/devices/{device_id}/shadow`
- `GET /v5/iot/{project_id}/devices/{device_id}`
- `POST /v5/iot/{project_id}/devices/{device_id}/commands`

### 4.2 页面能力

- 实时状态查看
- 远程命令下发（开关/投喂/配置）
- AP 配网辅助（读状态、扫网、提交 Wi-Fi）
- 历史与日报（本地聚合演示）

---

## 5. 云端固定参数

固定参数统一收口在：

- `PetHome_APP/entry/src/main/ets/config/iotdaDefaults.ets`
- `PetHome_Device/include/secrets.h`
- `docs/HuaweiCloud.MD`

---

## 6. 质量门禁

- 代码门禁：

```bash
python scripts/code_guard.py
```

- 约束：
  - 单文件 <= 1000 行
  - 单函数 <= 200 行

---

## 7. 联调启动顺序

1. 烧录固件并确认设备上线 IoTDA。  
2. 启动 App，直接调用 IoTDA Endpoint。  
3. 按 `docs/Acceptance_Checklist.MD` 做端到端验收。  
