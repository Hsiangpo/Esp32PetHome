#ifndef PET_HOME_BOARD_PINS_H_
#define PET_HOME_BOARD_PINS_H_

namespace board_pins {

// I2C 总线：SHT31/SGP30/BH1750/OLED 共用
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 9;

// 执行器引脚
constexpr int kServoPwm = 4;
constexpr int kPumpRelay = 5;
constexpr int kFanRelay = 6;
constexpr int kLedLight = 7;

// 采样相关引脚
constexpr int kFoodScaleDt = 16;
constexpr int kFoodScaleSck = 17;
constexpr int kWaterLevelDigital = 18;
constexpr int kUserButton = 15;

}  // namespace board_pins

#endif  // PET_HOME_BOARD_PINS_H_
