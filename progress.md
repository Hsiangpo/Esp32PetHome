# 进度日志

- 已完成 App 命令回执链路修复：新增 `ACCEPTED` 后轮询追踪，收敛最终执行态。
- 已完成 App 在线状态判定修复：优先设备详情 `status`，影子字段仅做兜底。
- 已完成设备侧温湿度滑动窗口容量修复：`kEnvHistoryCapacity` 从 16 提升到 24。
- 已扩展设备侧控制规则单元测试用例。
- 已完成验证：`code_guard`、设备构建、鸿蒙 API17 构建全部通过。
- 当前环境限制：
  - 缺少 `gcc/g++`，无法运行 `platformio native` 测试。
  - 缺少测试串口端口，无法执行 `esp32s3` 测试运行阶段。
- 已完成文档一致性修订：docs/Wiring_Guide.MD、docs/PRD.MD、docs/TECH.MD、docs/Acceptance_Checklist.MD、docs/Interface.MD。
- 已新增计划留档：docs/Plan/202603090044_接线与硬件文档修正方案.md。
- 已完成 GPIO4 标准 RC 舵机 PWM 改造，投喂动作为“打开 -> 停留 -> 回位”。
- 已完成验证：python -m platformio test -d PetHome_Device -e esp32s3 -f test_actuator_manager --without-uploading --without-testing 通过。
- 已完成验证：python -m platformio run -d PetHome_Device -e esp32s3 通过。
- 已完成验证：python scripts/code_guard.py 通过。
