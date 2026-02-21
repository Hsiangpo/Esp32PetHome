export interface DeviceLatest {
  ts: number;
  temp_c: number;
  hum_rh: number;
  lux: number;
  food_g: number;
  water_level_state: 'LOW' | 'OK';
  voc_index: number;
  tvoc_ppb: number;
  fan_on: boolean;
  led_on: boolean;
  pump_on: boolean;
  servo_active: boolean;
  online: boolean;
  fw_version: string;
  last_error: string;
}

export interface DeviceEvent {
  ts: number;
  event_type: string;
  level: 'INFO' | 'WARN' | 'ERROR';
  detail: Record<string, string | number | boolean> | string;
}

export interface HistoryPoint {
  ts: number;
  value: number;
}

export interface DailyReport {
  date: string;
  feed_count: number;
  refill_count: number;
  alert_count: number;
  avg_temp_c: number;
  avg_hum_rh: number;
  avg_voc_index: number;
  notes: string[];
}

export interface CommandResponse {
  request_id: string;
  result: 'OK' | 'REJECTED' | 'FAILED';
  reason: string;
  ts: number;
}

export interface AppConfig {
  baseUrl: string;
  projectId: string;
  deviceId: string;
  ak: string;
  sk: string;
}
