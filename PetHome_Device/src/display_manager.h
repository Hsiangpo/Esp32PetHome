#ifndef PET_HOME_DISPLAY_MANAGER_H_
#define PET_HOME_DISPLAY_MANAGER_H_

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pet_home_types.h"
#include "device_config.h"

// OLED 显示页面枚举
enum class DisplayPage {
  kHome,       // 主页：粮食重量、水位、在线状态
  kEnv,        // 环境页：温度、湿度、光照
  kAir,        // 空气页：VOC 指数、风扇状态
  kSettings,   // 设置页：阈值参数编辑
  kAlerts      // 告警页：最近事件摘要
};

// 告警事件记录
struct EventRecord {
  uint32_t ts_sec = 0;       // 事件时间（秒级，用于 OLED 显示）
  char type[24] = {0};       // 事件类型
  char level[8] = {0};       // 级别：INFO / WARN / ERROR
};

// 告警环形缓冲（10 条）
static constexpr int kMaxEventRecords = 10;

class DisplayManager {
 public:
  void Begin();
  void Update(const SensorSnapshot& snapshot, const RuntimeStatus& status,
              const DeviceConfig& config, bool in_settings, bool editing);
  void NextPage();
  void SetPage(DisplayPage page);
  DisplayPage GetCurrentPage() const { return current_page_; }

  // 设置模式参数项切换与数值增减
  void NextSettingItem();
  void IncreaseSettingValue(DeviceConfig& config);
  void DecreaseSettingValue(DeviceConfig& config);
  int GetSettingIndex() const { return setting_index_; }
  void ResetSettingIndex() { setting_index_ = 0; }

  // 告警事件写入
  void PushEvent(const char* event_type, const char* level);

  // 告警翻阅
  void NextAlertPage();
  bool CanScrollAlertNext() const;

 private:
  void DrawHome(const SensorSnapshot& snapshot, const RuntimeStatus& status);
  void DrawEnv(const SensorSnapshot& snapshot);
  void DrawAir(const SensorSnapshot& snapshot, const RuntimeStatus& status);
  void DrawSettings(const DeviceConfig& config, bool in_settings, bool editing);
  void DrawAlerts();

  Adafruit_SSD1306 display_;
  DisplayPage current_page_ = DisplayPage::kHome;

  // 设置模式当前选中参数项索引
  int setting_index_ = 0;

  // 告警环形缓冲
  EventRecord event_ring_[kMaxEventRecords];
  int event_write_pos_ = 0;     // 下一个写入位置
  int event_count_ = 0;         // 当前有效记录数
  int alert_scroll_offset_ = 0; // 告警页翻阅偏移
};

#endif  // PET_HOME_DISPLAY_MANAGER_H_
