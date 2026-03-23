#ifndef PET_HOME_SECRETS_H_
#define PET_HOME_SECRETS_H_

// 课设演示口径：云端参数允许明文硬编码，避免将空值提交到云端。
// 实际联调前需确认以下参数与当前 IoTDA 实例一致。
namespace secrets {

constexpr char kMqttHost[] = "8cee850016.st1.iotda-device.cn-north-4.myhuaweicloud.com";
constexpr uint16_t kMqttPort = 1883;
constexpr char kDeviceId[] = "69982a337f2e6c302f55fae1_PetHome_01";
constexpr char kDeviceSecret[] = "c51179ff1a0ed9bc42e1e0280cd03692";
constexpr char kProjectId[] = "79d1f76b8f0e47fd934f3eb393bf2e7f";
constexpr char kIamAccessKey[] = "HPUAXBOD7O94OFVQKNST";
constexpr char kIamSecretKey[] = "ai2ept3hXzrYJF8eifMBCQ9udBli9K8X5lL3dSCY";

}  // namespace secrets

#endif  // PET_HOME_SECRETS_H_
