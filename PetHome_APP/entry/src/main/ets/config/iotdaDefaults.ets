export const IOTDA_DEFAULTS = {
  baseUrl: 'https://8cee850016.st1.iotda-app.cn-north-4.myhuaweicloud.com',
  projectId: '79d1f76b8f0e47fd934f3eb393bf2e7f',
  deviceId: '69982a337f2e6c302f55fae1_PetHome_01',
  ak: 'HPUA4JX3YFP88FSKLZ8N',
  sk: 'vZ2ggGBLX6ekyeZWDxopQM9J5DXmMSbUI5JNfgDg',
  deviceSecret: 'c51179ff1a0ed9bc42e1e0280cd03692',
  deviceMqttHost: '8cee850016.st1.iotda-device.cn-north-4.myhuaweicloud.com',
  deviceMqttPort: 1883,
} as const;

export const IOTDA_SERVICE_ID_CONTROL = 'pet_home_control';
export const IOTDA_SERVICE_ID_CONFIG = 'pet_home_config';
export const IOTDA_SERVICE_ID_PROPERTY = 'PetHome';
export const IOTDA_COMMAND_NAME_CONTROL = 'control';
export const IOTDA_COMMAND_NAME_SET_CONFIG = 'set_config';
export const IOTDA_COMMAND_NAME_SET_NIGHT_WINDOW = 'set_night_window';
