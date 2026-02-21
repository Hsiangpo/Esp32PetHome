#include "button_manager.h"
#include "board_pins.h"
#include <Arduino.h>

void ButtonManager::Begin() {
  btn_ = OneButton(board_pins::kUserButton, true, true);
  btn_.attachLongPressStart(StartHandler, this);
  btn_.attachLongPressStop(StopHandler, this);
}

void ButtonManager::Loop() {
  btn_.tick();
}

void ButtonManager::SetOnLongPress2s(Callback cb) {
  long2_ = cb;
}

void ButtonManager::SetOnLongPress5s(Callback cb) {
  long5_ = cb;
}

void ButtonManager::StartHandler(void* ptr) {
  ButtonManager* self = static_cast<ButtonManager*>(ptr);
  // btn.attachLongPressStart 在 1000ms 后触发，真正按下时间是 1000ms 前
  self->press_start_ = (millis() > 1000) ? (millis() - 1000) : millis();
}

void ButtonManager::StopHandler(void* ptr) {
  ButtonManager* self = static_cast<ButtonManager*>(ptr);
  uint32_t dur = millis() - self->press_start_;
  if (dur >= 5000) {
    if (self->long5_) self->long5_();
  } else if (dur >= 2000) {
    if (self->long2_) self->long2_();
  }
}
