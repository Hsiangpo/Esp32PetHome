export interface IotdaDefaults {
  baseUrl: string;
  region: string;
  instanceId: string;
  projectId: string;
  deviceId: string;
  ak: string;
  sk: string;
  deviceSecret: string;
  deviceMqttHost: string;
  deviceMqttPort: number;
}

export const IOTDA_DEFAULTS: IotdaDefaults = {
  baseUrl: 'https://8cee850016.st1.iotda-app.cn-north-4.myhuaweicloud.com',
  region: 'cn-north-4',
  instanceId: '8145412f-8cb1-46a1-8755-d692c4dc1b65',
  projectId: '79d1f76b8f0e47fd934f3eb393bf2e7f',
  deviceId: '69982a337f2e6c302f55fae1_PetHome_01',
  ak: 'HPUAXBOD7O94OFVQKNST',
  sk: 'ai2ept3hXzrYJF8eifMBCQ9udBli9K8X5lL3dSCY',
  deviceSecret: 'c51179ff1a0ed9bc42e1e0280cd03692',
  deviceMqttHost: '8cee850016.st1.iotda-device.cn-north-4.myhuaweicloud.com',
  deviceMqttPort: 1883,
};

export const IOTDA_SERVICE_ID_CONTROL = 'pet_home_control';
export const IOTDA_SERVICE_ID_CONFIG = 'pet_home_config';
export const IOTDA_SERVICE_ID_PROPERTY = 'PetHome';
export const IOTDA_COMMAND_NAME_CONTROL = 'control';
export const IOTDA_COMMAND_NAME_SET_CONFIG = 'set_config';
export const IOTDA_COMMAND_NAME_SET_NIGHT_WINDOW = 'set_night_window';
