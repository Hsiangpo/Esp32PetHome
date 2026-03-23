export interface RangeRule {
  min: number;
  max: number;
  step: number;
}

export interface FeedCommandParams {
  feed_target_g: number;
}

export interface CommandPayload {
  request_id: string;
  ts: number;
  cmd: string;
  params: FeedCommandParams;
}

export interface ProvisionConfigPayload {
  ssid: string;
  pwd: string;
}

const HH_MM_PATTERN = /^([01]\d|2[0-3]):([0-5]\d)$/;

function clampNumber(value: number, min: number, max: number): number {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

function getRangeRule(key: string): RangeRule | undefined {
  switch (key) {
    case 'sample_interval_ms':
      return { min: 500, max: 5000, step: 100 };
    case 'publish_interval_ms':
      return { min: 10000, max: 300000, step: 1000 };
    case 'change_publish_min_interval_ms':
      return { min: 1000, max: 60000, step: 500 };
    case 'food_low_threshold_g':
      return { min: 10, max: 500, step: 10 };
    case 'feed_target_g':
      return { min: 10, max: 150, step: 5 };
    case 'feed_max_run_ms':
      return { min: 3000, max: 60000, step: 1000 };
    case 'water_debounce_ms':
      return { min: 500, max: 10000, step: 500 };
    case 'water_ok_hold_ms':
      return { min: 500, max: 10000, step: 500 };
    case 'pump_max_run_ms':
      return { min: 5000, max: 120000, step: 1000 };
    case 'pump_cooldown_ms':
      return { min: 5000, max: 180000, step: 1000 };
    case 'voc_on_threshold':
      return { min: 50, max: 500, step: 10 };
    case 'voc_off_threshold':
      return { min: 0, max: 450, step: 10 };
    case 'voc_debounce_ms':
      return { min: 500, max: 30000, step: 500 };
    case 'voc_ok_hold_ms':
      return { min: 500, max: 60000, step: 500 };
    case 'fan_min_on_ms':
      return { min: 5000, max: 300000, step: 1000 };
    case 'fan_max_on_ms':
      return { min: 60000, max: 7200000, step: 60000 };
    case 'light_on_threshold':
      return { min: 10, max: 1000, step: 10 };
    case 'light_off_threshold':
      return { min: 20, max: 1200, step: 10 };
    case 'temp_high_threshold_c':
      return { min: 20, max: 40, step: 1 };
    case 'temp_low_threshold_c':
      return { min: 0, max: 20, step: 1 };
    case 'hum_high_threshold_rh':
      return { min: 40, max: 90, step: 1 };
    case 'hum_low_threshold_rh':
      return { min: 10, max: 60, step: 1 };
    default:
      return undefined;
  }
}

export function clampConfigValue(key: string, value: number): number {
  const rule = getRangeRule(key);
  if (rule === undefined) {
    return value;
  }
  return clampNumber(value, rule.min, rule.max);
}

export function buildFeedCommandPayload(requestId: string, feedTarget: number): CommandPayload {
  const payload: CommandPayload = {
    request_id: requestId,
    ts: Date.now(),
    cmd: 'feed_once',
    params: {
      feed_target_g: clampConfigValue('feed_target_g', feedTarget),
    },
  };
  return payload;
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
  const payload: ProvisionConfigPayload = {
    ssid: ssid.trim(),
    pwd,
  };
  return payload;
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
