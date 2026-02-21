#ifndef PET_HOME_BUTTON_MANAGER_H_
#define PET_HOME_BUTTON_MANAGER_H_

#include <OneButton.h>

class ButtonManager {
 public:
  typedef void (*Callback)();

  void Begin();
  void Loop();

  void SetOnClick(Callback cb) { btn_.attachClick(cb); }
  void SetOnDoubleClick(Callback cb) { btn_.attachDoubleClick(cb); }
  void SetOnLongPress2s(Callback cb);
  void SetOnLongPress5s(Callback cb);

 private:
  OneButton btn_;
  Callback long2_ = nullptr;
  Callback long5_ = nullptr;
  uint32_t press_start_ = 0;

  static void StartHandler(void* ptr);
  static void StopHandler(void* ptr);
};

#endif  // PET_HOME_BUTTON_MANAGER_H_
