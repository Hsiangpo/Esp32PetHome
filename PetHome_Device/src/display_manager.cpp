#include "display_manager.h"
#include <Wire.h>
#include <time.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "board_pins.h"

// U8g2 字体渲染器（叠加在 Adafruit GFX 上，支持中文）
static U8G2_FOR_ADAFRUIT_GFX u8g2_font;

// 设置模式可编辑参数项定义
struct SettingItemDef {
  const char* label;          // 显示标签（中文 UTF-8）
  int DeviceConfig::*int_ptr; // 指向 DeviceConfig 的 int 成员
  int min_val;
  int max_val;
  int step;
};

// 覆盖 PRD 要求的 10 个参数
static const SettingItemDef kSettingItems[] = {
    {"低粮阈值(g)",   &DeviceConfig::food_low_threshold_g,  10,  500,  10},
    {"投喂量(g)",     &DeviceConfig::feed_target_g,          10,  150,   5},
    {"补光开(lux)",   &DeviceConfig::light_on_threshold,     10, 1000,  10},
    {"补光关(lux)",   &DeviceConfig::light_off_threshold,    20, 1200,  10},
    {"风扇开(voc)",   &DeviceConfig::voc_on_threshold,       50,  500,  10},
    {"风扇关(voc)",   &DeviceConfig::voc_off_threshold,       0,  450,  10},
    {"温度高(C)",     &DeviceConfig::temp_high_threshold_c,  20,   40,   1},
    {"温度低(C)",     &DeviceConfig::temp_low_threshold_c,    0,   20,   1},
    {"湿度高(%)",     &DeviceConfig::hum_high_threshold_rh,  40,   90,   1},
    {"湿度低(%)",     &DeviceConfig::hum_low_threshold_rh,   10,   60,   1},
};
static constexpr int kSettingItemCount = sizeof(kSettingItems) / sizeof(kSettingItems[0]);

// 使用 U8g2 绘制中文字符串
static void DrawChinese(int x, int y, const char* utf8_text) {
  u8g2_font.setCursor(x, y);
  u8g2_font.print(utf8_text);
}

void DisplayManager::Begin() {
  display_ = Adafruit_SSD1306(128, 64, &Wire, -1);
  if (display_.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    u8g2_font.begin(display_);
    u8g2_font.setFont(u8g2_font_wqy12_t_chinese1);
    u8g2_font.setForegroundColor(SSD1306_WHITE);
    display_.clearDisplay();
    DrawChinese(0, 14, "宠物屋启动中...");
    display_.display();
  }
}

void DisplayManager::Update(const SensorSnapshot& snapshot,
                            const RuntimeStatus& status,
                            const DeviceConfig& config, bool in_settings, bool editing) {
  display_.clearDisplay();
  u8g2_font.setFont(u8g2_font_wqy12_t_chinese1);
  u8g2_font.setForegroundColor(SSD1306_WHITE);

  switch (current_page_) {
    case DisplayPage::kHome:     DrawHome(snapshot, status);         break;
    case DisplayPage::kEnv:      DrawEnv(snapshot);                  break;
    case DisplayPage::kAir:      DrawAir(snapshot, status);          break;
    case DisplayPage::kSettings: DrawSettings(config, in_settings, editing);  break;
    case DisplayPage::kAlerts:   DrawAlerts();                       break;
  }
  display_.display();
}

void DisplayManager::NextPage() {
  int p = static_cast<int>(current_page_);
  p = (p + 1) % 5;
  current_page_ = static_cast<DisplayPage>(p);
  alert_scroll_offset_ = 0;
}

void DisplayManager::SetPage(DisplayPage page) {
  current_page_ = page;
}

// ---- 设置模式参数切换 ----
void DisplayManager::NextSettingItem() {
  setting_index_ = (setting_index_ + 1) % kSettingItemCount;
}

void DisplayManager::IncreaseSettingValue(DeviceConfig& config) {
  if (setting_index_ < 0 || setting_index_ >= kSettingItemCount) return;
  const auto& item = kSettingItems[setting_index_];
  int& val = config.*(item.int_ptr);
  val += item.step;
  if (val > item.max_val) val = item.max_val;
}

void DisplayManager::DecreaseSettingValue(DeviceConfig& config) {
  if (setting_index_ < 0 || setting_index_ >= kSettingItemCount) return;
  const auto& item = kSettingItems[setting_index_];
  int& val = config.*(item.int_ptr);
  val -= item.step;
  if (val < item.min_val) val = item.min_val;
}

// ---- 告警事件写入（环形缓冲）----
void DisplayManager::PushEvent(const char* event_type, const char* level) {
  EventRecord& rec = event_ring_[event_write_pos_];
  // 获取秒级时间戳
  time_t now = time(nullptr);
  rec.ts_sec = (now > 1700000000) ? static_cast<uint32_t>(now) : (millis() / 1000);
  strncpy(rec.type, event_type, sizeof(rec.type) - 1);
  rec.type[sizeof(rec.type) - 1] = '\0';
  strncpy(rec.level, level, sizeof(rec.level) - 1);
  rec.level[sizeof(rec.level) - 1] = '\0';

  event_write_pos_ = (event_write_pos_ + 1) % kMaxEventRecords;
  if (event_count_ < kMaxEventRecords) event_count_++;
}

// ---- 告警翻阅 ----
void DisplayManager::NextAlertPage() {
  if (event_count_ > 4) {
    alert_scroll_offset_++;
    if (alert_scroll_offset_ > event_count_ - 4) {
      alert_scroll_offset_ = event_count_ - 4;
    }
  }
}

bool DisplayManager::CanScrollAlertNext() const {
  return event_count_ > 4 && alert_scroll_offset_ < event_count_ - 4;
}

// ---- 绘制主页 ----
void DisplayManager::DrawHome(const SensorSnapshot& snapshot,
                              const RuntimeStatus& status) {
  DrawChinese(0, 14, "【主页】");
  char buf[32];
  snprintf(buf, sizeof(buf), "粮食: %.0f g", snapshot.food_g);
  DrawChinese(0, 28, buf);
  snprintf(buf, sizeof(buf), "水位: %s", snapshot.water_low ? "偏低" : "正常");
  DrawChinese(0, 42, buf);
  snprintf(buf, sizeof(buf), "在线: %s", status.online ? "是" : "否");
  DrawChinese(0, 56, buf);
}

// ---- 绘制环境页 ----
void DisplayManager::DrawEnv(const SensorSnapshot& snapshot) {
  DrawChinese(0, 14, "【环境】");
  char buf[32];
  snprintf(buf, sizeof(buf), "温度: %.1f C", snapshot.temp_c);
  DrawChinese(0, 28, buf);
  snprintf(buf, sizeof(buf), "湿度: %.1f %%", snapshot.hum_rh);
  DrawChinese(0, 42, buf);
  snprintf(buf, sizeof(buf), "光照: %.0f lx", snapshot.lux);
  DrawChinese(0, 56, buf);
}

// ---- 绘制空气页 ----
void DisplayManager::DrawAir(const SensorSnapshot& snapshot,
                             const RuntimeStatus& status) {
  DrawChinese(0, 14, "【空气】");
  char buf[32];
  snprintf(buf, sizeof(buf), "气味: %d", snapshot.voc_index);
  DrawChinese(0, 28, buf);
  snprintf(buf, sizeof(buf), "TVOC: %d ppb", snapshot.tvoc_ppb);
  DrawChinese(0, 42, buf);
  snprintf(buf, sizeof(buf), "风扇: %s", status.fan_on ? "开" : "关");
  DrawChinese(0, 56, buf);
}

// ---- 绘制设置页（含参数编辑）----
void DisplayManager::DrawSettings(const DeviceConfig& config, bool in_settings, bool editing) {
  if (in_settings) {
    DrawChinese(0, 14, editing ? "【编辑】" : "【选择】");
  } else {
    DrawChinese(0, 14, "【设置】");
  }

  // 显示 4 行参数（从 setting_index_ 开始）
  const int start = (setting_index_ / 4) * 4;
  for (int i = start; i < start + 4 && i < kSettingItemCount; i++) {
    const auto& item = kSettingItems[i];
    const int val = config.*(item.int_ptr);
    char line[32];
    char prefix = ' ';
    if (in_settings && i == setting_index_) {
      prefix = editing ? '*' : '>';
    }
    snprintf(line, sizeof(line), "%c%s:%d", prefix, item.label, val);
    int y = 28 + (i - start) * 10;
    DrawChinese(0, y, line);
  }
}

// ---- 绘制告警页 ----
void DisplayManager::DrawAlerts() {
  DrawChinese(0, 14, "【告警】");

  if (event_count_ == 0) {
    DrawChinese(0, 28, "无告警记录");
    return;
  }

  // 按时间倒序显示（最新的在前）
  const int show_count = (event_count_ < 4) ? event_count_ : 4;
  for (int i = 0; i < show_count; i++) {
    int idx = (event_write_pos_ - 1 - alert_scroll_offset_ - i + kMaxEventRecords * 2) % kMaxEventRecords;
    if (idx < 0) idx += kMaxEventRecords;
    const EventRecord& rec = event_ring_[idx];

    // 格式化时间 HH:MM
    char time_str[6] = "??:??";
    if (rec.ts_sec > 1700000000) {
      time_t t = static_cast<time_t>(rec.ts_sec);
      struct tm tinfo;
      localtime_r(&t, &tinfo);
      snprintf(time_str, sizeof(time_str), "%02d:%02d", tinfo.tm_hour, tinfo.tm_min);
    }

    char line[32];
    snprintf(line, sizeof(line), "%s %s %.7s", time_str, rec.level, rec.type);
    DrawChinese(0, 26 + i * 10, line);
  }
}
