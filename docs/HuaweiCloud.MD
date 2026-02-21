# 华为云 IoTDA 配置与实参（已执行）

本文档记录 Esp32PetHome 在华为云 IoTDA 的真实参数与执行结果。
按 `AGENTS.MD` 约束：AK/SK、设备密钥、三元组允许明文记录并在项目中硬编码。

---

## 1. 本次执行结果（2026-02-20）

- 新建独立产品：`智能宠物屋`（`product_id=69982a337f2e6c302f55fae1`）
- 新建独立设备：`PetHome_01`（`device_id=69982a337f2e6c302f55fae1_PetHome_01`）
- 已完成新产品服务模型（PetHome 命名）
- 已确认旧水族箱产品和设备未被改动

---

## 2. 云端基础标识

| 参数 | 值 |
| --- | --- |
| `region` | `cn-north-4` |
| `instance_id` | `8145412f-8cb1-46a1-8755-d692c4dc1b65` |
| `project_id` | `79d1f76b8f0e47fd934f3eb393bf2e7f` |
| `app_id` | `96daea436f0141a8a292102edde1eab9` |
| `app_name` | `DefaultApp_6902q62q` |

---

## 3. 新产品与新设备（PetHome）

### 3.1 产品

| 参数 | 值 |
| --- | --- |
| `product_id` | `69982a337f2e6c302f55fae1` |
| `product_name` | `智能宠物屋` |
| `device_type` | `ESP32-PetHome` |
| `protocol_type` | `MQTT` |
| `data_format` | `json` |

### 3.2 设备

| 参数 | 值 |
| --- | --- |
| `node_id` | `PetHome_01` |
| `device_id` | `69982a337f2e6c302f55fae1_PetHome_01` |
| `device_secret` | `c51179ff1a0ed9bc42e1e0280cd03692` |
| `status_at_create` | `INACTIVE` |

---

## 4. 旧产品资源状态（只核对，不改动）

| 资源 | 状态 |
| --- | --- |
| 旧产品 `690237639798273cc4fd09cb` | 保留 |
| 旧设备（历史主设备） | 保留 |

说明：本次新增均在 PetHome 新产品下完成，不覆盖旧产品模型，不删除旧设备。

---

## 5. 接入地址

### 5.1 设备接入

| 协议 | 端口 | 域名 |
| --- | --- | --- |
| `MQTT` | `1883` | `8cee850016.st1.iotda-device.cn-north-4.myhuaweicloud.com` |
| `MQTTS` | `8883` | `8cee850016.st1.iotda-device.cn-north-4.myhuaweicloud.com` |
| `HTTPS` | `443` | `8cee850016.st1.iotda-device.cn-north-4.myhuaweicloud.com` |

### 5.2 应用接入

| 协议 | 端口 | 域名 |
| --- | --- | --- |
| `HTTPS` | `443` | `8cee850016.st1.iotda-app.cn-north-4.myhuaweicloud.com` |
| `AMQPS` | `5671` | `8cee850016.st1.iotda-app.cn-north-4.myhuaweicloud.com` |
| `MQTTS` | `8883` | `8cee850016.st1.iotda-app.cn-north-4.myhuaweicloud.com` |

---

## 6. 鉴权参数（硬编码口径）

### 6.1 设备侧

- `device_id=69982a337f2e6c302f55fae1_PetHome_01`
- `device_secret=c51179ff1a0ed9bc42e1e0280cd03692`
- `project_id=79d1f76b8f0e47fd934f3eb393bf2e7f`

### 6.2 App 侧（IoTDA OpenAPI）

- `AK=HPUA4JX3YFP88FSKLZ8N`
- `SK=vZ2ggGBLX6ekyeZWDxopQM9J5DXmMSbUI5JNfgDg`

---

## 7. 当前云侧服务模型（PetHome 命名）

### 7.1 `PetHome`（属性服务）

- `ts`
- `temp_c`
- `hum_rh`
- `lux`
- `food_g`
- `water_level_state`
- `voc_index`
- `tvoc_ppb`
- `fan_on`
- `led_on`
- `pump_on`
- `servo_active`
- `online`
- `fw_version`
- `last_error`

### 7.2 `pet_home_control.control`（控制命令）

- `led_on`
- `fan_on`
- `pump_on`
- `feed`
- `feed_target_g`

### 7.3 `pet_home_config.set_config`（配置命令）

- `food_low_threshold_g`
- `feed_target_g`
- `feed_max_run_ms`
- `water_debounce_ms`
- `water_ok_hold_ms`
- `pump_max_run_ms`
- `pump_cooldown_ms`
- `voc_on_threshold`
- `voc_off_threshold`
- `voc_debounce_ms`
- `voc_ok_hold_ms`
- `fan_min_on_ms`
- `fan_max_on_ms`
- `light_on_threshold`
- `light_off_threshold`
- `temp_high_threshold_c`
- `temp_low_threshold_c`
- `hum_high_threshold_rh`
- `hum_low_threshold_rh`
- `night_mode_enabled`

### 7.4 `pet_home_config.set_night_window`（夜间时间窗命令）

- `night_mode_start`
- `night_mode_end`

---

## 8. 已回填到代码的位置

- `PetHome_Device/include/secrets.h`
  - `kDeviceId=69982a337f2e6c302f55fae1_PetHome_01`
  - `kDeviceSecret=c51179ff1a0ed9bc42e1e0280cd03692`
  - `kProjectId=79d1f76b8f0e47fd934f3eb393bf2e7f`
  - `kIamAccessKey=HPUA4JX3YFP88FSKLZ8N`
  - `kIamSecretKey=vZ2ggGBLX6ekyeZWDxopQM9J5DXmMSbUI5JNfgDg`
- `PetHome_APP/entry/src/main/ets/config/iotdaDefaults.ets`
  - `baseUrl/projectId/deviceId/ak/sk/deviceSecret`
- `PetHome_APP/entry/src/main/ets/service/PetHomeApi.ets`
  - `huaweiCloudFixedConfig.productId=69982a337f2e6c302f55fae1`

---

## 9. 架构口径

- App 直连 IoTDA App Endpoint：
  - `https://8cee850016.st1.iotda-app.cn-north-4.myhuaweicloud.com`
- App 使用 AK/SK 进行 `SDK-HMAC-SHA256` 签名调用 IoTDA REST。
- 不再使用本地 `127.0.0.1` 后端作为默认联调路径。
