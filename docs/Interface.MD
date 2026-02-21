# 接口协议文档（Interface）

本文档定义智能宠物屋项目中设备、云端、应用三端的统一交互协议。  
字段命名统一使用 `snake_case`，时间统一使用 `ts`（Unix 毫秒）。

---

## 1. MQTT Topic 约定（IoTDA 原生）

| 方向 | Topic |
| --- | --- |
| 设备上报属性 | `$oc/devices/{device_id}/sys/properties/report` |
| 设备上报事件 | `$oc/devices/{device_id}/sys/events/up` |
| 设备订阅命令 | `$oc/devices/{device_id}/sys/commands/#` |
| 设备命令响应 | `$oc/devices/{device_id}/sys/commands/response/request_id={request_id}` |

---

## 2. 属性上报（sys/properties/report）

### 2.1 Payload 示例

```json
{
  "services": [
    {
      "service_id": "PetHome",
      "event_time": "20260220T120000Z",
      "properties": {
        "ts": 1739980800123,
        "temp_c": 24.6,
        "hum_rh": 56.3,
        "lux": 92.0,
        "food_g": 38.5,
        "water_level_state": "LOW",
        "voc_index": 131,
        "tvoc_ppb": 262,
        "fan_on": false,
        "led_on": true,
        "pump_on": false,
        "servo_active": false,
        "online": true,
        "fw_version": "v1.0.0",
        "last_error": ""
      }
    }
  ]
}
```

---

## 3. 事件上报（sys/events/up）

### 3.1 Payload 示例

```json
{
  "services": [
    {
      "service_id": "PetHome",
      "event_type": "WATER_REFILL_FAILED",
      "event_time": "20260220T120030Z",
      "paras": {
        "level": "WARN",
        "detail": "pump_timeout",
        "ts": 1739980822456
      }
    }
  ]
}
```

### 3.2 事件枚举

- `LOW_FOOD`
- `LOW_WATER`
- `WATER_REFILL_FAILED`
- `FEED_FAILED`
- `SENSOR_FAULT`
- `WIFI_OFFLINE`
- `CLOUD_OFFLINE`
- `FAN_OVERTIME`
- `CONFIG_CONFIRMED`
- `CONFIG_RESET_DEFAULT`

---

## 4. 云端下发命令（sys/commands/request_id=...）

### 4.1 控制命令 `pet_home_control.control`

```json
{
  "object_device_id": "69982a337f2e6c302f55fae1_PetHome_01",
  "service_id": "pet_home_control",
  "command_name": "control",
  "paras": {
    "led_on": true,
    "fan_on": false,
    "pump_on": false,
    "feed": false,
    "feed_target_g": 50
  }
}
```

### 4.2 配置命令 `pet_home_config.set_config`

```json
{
  "object_device_id": "69982a337f2e6c302f55fae1_PetHome_01",
  "service_id": "pet_home_config",
  "command_name": "set_config",
  "paras": {
    "food_low_threshold_g": 60,
    "feed_target_g": 50,
    "pump_max_run_ms": 30000,
    "temp_high_threshold_c": 30,
    "temp_low_threshold_c": 10,
    "hum_high_threshold_rh": 70,
    "hum_low_threshold_rh": 30
  }
}
```

### 4.3 夜间时间窗命令 `pet_home_config.set_night_window`

```json
{
  "object_device_id": "69982a337f2e6c302f55fae1_PetHome_01",
  "service_id": "pet_home_config",
  "command_name": "set_night_window",
  "paras": {
    "night_mode_start": "22:00",
    "night_mode_end": "07:00"
  }
}
```

---

## 5. 设备命令响应（sys/commands/response/request_id=...）

### 5.1 Payload 示例

```json
{
  "result_code": 0,
  "response_name": "control_response",
  "paras": {
    "result": "success",
    "reason": "OK",
    "ts": 1739980802100
  }
}
```

### 5.2 `result_code` 约定

- `0`：执行成功
- `1`：请求被拒绝（如互锁冲突，返回 REJECTED）
- `2`：执行失败（如动作执行超时等，返回 FAILED）

示例说明：
- 对于 `feed_once` 或带 `feed` 的控制命令，若与水泵产生互锁冲突，直接拒绝，同步回执 `result_code: 1`，reason 为 `FEED_INTERLOCK_REJECTED`；
- 若投喂动作开始，设备将等待执行完毕或超时后再发出命令真实回执；
- 若投喂动作超出 `feed_max_run_ms` 仍未完成，将返回 `result_code: 2` (FAILED)，reason 为 `FEED_TIMEOUT`，并同步触发事件 `FEED_FAILED`。

---

## 6. App 直连 IoTDA REST 接口（当前实现）

基础参数（固定内置）：

- `baseUrl=https://8cee850016.st1.iotda-app.cn-north-4.myhuaweicloud.com`
- `project_id=79d1f76b8f0e47fd934f3eb393bf2e7f`
- `device_id=69982a337f2e6c302f55fae1_PetHome_01`
- `ak/sk`：见 `docs/HuaweiCloud.MD`

接口：

- `GET /v5/iot/{project_id}/devices/{device_id}/shadow`
- `GET /v5/iot/{project_id}/devices/{device_id}`
- `POST /v5/iot/{project_id}/devices/{device_id}/commands`
- `GET /v5/iot/{project_id}/devices` （获取华为云项目下设备列表）
- `GET /v5/iot/{project_id}/devices/{device_id}/messages` （获取历史消息/指标数据）

签名方式：

- `SDK-HMAC-SHA256`
- `X-Sdk-Date` + `Authorization`

### 6.1 设备列表查询（云端接口）

端点: `GET /v5/iot/{project_id}/devices`

响应示例:
```json
{
  "devices": [
    {
      "device_id": "69982a337f2e6c302f55fae1_PetHome_01",
      "device_name": "PetHome_01",
      "status": "ONLINE"
    }
  ],
  "page": { "count": 1 }
}
```

### 6.2 历史数据曲线查询（带时间范围）

端点: `GET /v5/iot/{project_id}/devices/{device_id}/messages?limit=200&start_time={start_time}&end_time={end_time}`

请求参数说明:
- `start_time` / `end_time`: 格式必须为紧凑型 ISO 8601，如 `20260220T000000Z`。
- `limit`: 返回条数限制，最大可达 200。
- App 本地查询时还可附加逻辑粒度参数（`step`），但不由 IoTDA 解析，App 在拿到批量数据后自行按时间步长下采样。

---

## 7. 设备内置 Web 配网（AP Provisioning）

该接口为设备在初始化时充当 AP（通常内网 IP 为 `192.168.4.1` 或热点网关分配 IP）时提供的短生命周期 HTTP 接口。

### 7.1 读取热点状态 (`GET /status`)

```json
{
  "ap_ssid": "PetHome_Setup_xxxx",
  "ap_pwd": "",
  "provisioned": false
}
```

### 7.2 扫描可用 Wi-Fi (`GET /scan`)

```json
{
  "networks": [
    {"ssid": "HomeWiFi", "rssi": -60},
    {"ssid": "Guest_2.4G", "rssi": -85}
  ]
}
```

### 7.3 下发配网参数 (`POST /config`)

Req Payload:

```json
{
  "ssid": "HomeWiFi",
  "pwd": "password123!"
}
```

Res Payload:

```json
{
  "ok": true,
  "message": "saved"
}
```

错误时返回：

```json
{
  "ok": false,
  "reason": "ssid_or_pwd_missing"
}
```
