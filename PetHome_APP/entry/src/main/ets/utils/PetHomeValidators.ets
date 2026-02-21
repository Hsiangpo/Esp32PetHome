export interface RangeRule {
  min: number;
  max: number;
  step: number;
}

export const configRangeMap: Record<string, RangeRule> = {
  food_low_threshold_g: { min: 10, max: 500, step: 10 },
  feed_target_g: { min: 10, max: 150, step: 5 },
  feed_max_run_ms: { min: 3000, max: 60000, step: 1000 },
  water_debounce_ms: { min: 500, max: 10000, step: 500 },
  water_ok_hold_ms: { min: 500, max: 10000, step: 500 },
  pump_max_run_ms: { min: 5000, max: 120000, step: 1000 },
  pump_cooldown_ms: { min: 5000, max: 180000, step: 1000 },
  voc_on_threshold: { min: 50, max: 500, step: 10 },
  voc_off_threshold: { min: 0, max: 450, step: 10 },
  voc_debounce_ms: { min: 500, max: 30000, step: 500 },
  voc_ok_hold_ms: { min: 500, max: 60000, step: 500 },
  fan_min_on_ms: { min: 5000, max: 300000, step: 1000 },
  fan_max_on_ms: { min: 60000, max: 7200000, step: 60000 },
  light_on_threshold: { min: 10, max: 1000, step: 10 },
  light_off_threshold: { min: 20, max: 1200, step: 10 },
  temp_high_threshold_c: { min: 20, max: 40, step: 1 },
  temp_low_threshold_c: { min: 0, max: 20, step: 1 },
  hum_high_threshold_rh: { min: 40, max: 90, step: 1 },
  hum_low_threshold_rh: { min: 10, max: 60, step: 1 },
};

function clampNumber(value: number, min: number, max: number): number {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

export function clampConfigValue(key: string, value: number): number {
  const rule: RangeRule | undefined = configRangeMap[key];
  if (rule === undefined) {
    return value;
  }
  return clampNumber(value, rule.min, rule.max);
}

export interface CommandPayload {
  request_id: string;
  ts: number;
  cmd: string;
  params: Record<string, number | boolean | string>;
}

export interface ProvisionConfigPayload {
  ssid: string;
  pwd: string;
}

const HH_MM_PATTERN = /^([01]\d|2[0-3]):([0-5]\d)$/;

export function buildFeedCommandPayload(requestId: string, feedTarget: number): CommandPayload {
  return {
    request_id: requestId,
    ts: Date.now(),
    cmd: 'feed_once',
    params: {
      feed_target_g: clampConfigValue('feed_target_g', feedTarget),
    },
  };
}

export function normalizeApGateway(input: string): string {
  const trimmed = input.trim();
  if (trimmed.length === 0) {
    return 'http://192.168.4.1';
  }
  let normalized = trimmed;
  while (normalized.endsWith('/')) {
    normalized = normalized.slice(0, normalized.length - 1);
  }
  return normalized;
}

export function buildProvisionConfigPayload(ssid: string, pwd: string): ProvisionConfigPayload {
  return {
    ssid: ssid.trim(),
    pwd,
  };
}

export function isValidTimeValue(input: string): boolean {
  return HH_MM_PATTERN.test(input.trim());
}

export function normalizeTimeValue(input: string, fallback: string): string {
  const trimmed = input.trim();
  if (isValidTimeValue(trimmed)) {
    return trimmed;
  }
  return fallback;
}
