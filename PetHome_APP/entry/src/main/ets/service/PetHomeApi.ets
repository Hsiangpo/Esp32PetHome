import http from '@ohos.net.http';
import {
  AppConfig,
  CommandResponse,
  DailyReport,
  DeviceEvent,
  DeviceLatest,
  HistoryPoint,
} from '../model/PetHomeModels';
import {
  buildProvisionConfigPayload,
  clampConfigValue,
  normalizeApGateway,
} from '../utils/PetHomeValidators';
import {
  IOTDA_COMMAND_NAME_CONTROL,
  IOTDA_COMMAND_NAME_SET_CONFIG,
  IOTDA_COMMAND_NAME_SET_NIGHT_WINDOW,
  IOTDA_DEFAULTS,
  IOTDA_SERVICE_ID_CONFIG,
  IOTDA_SERVICE_ID_CONTROL,
} from '../config/iotdaDefaults';
import { buildAuthorization, buildXSdkDate } from '../api/iotdaSigner';

export const huaweiCloudFixedConfig = {
  region: 'cn-north-4',
  instanceId: '8145412f-8cb1-46a1-8755-d692c4dc1b65',
  projectId: IOTDA_DEFAULTS.projectId,
  iamAk: IOTDA_DEFAULTS.ak,
  iamSk: IOTDA_DEFAULTS.sk,
  appId: '96daea436f0141a8a292102edde1eab9',
  productId: '69982a337f2e6c302f55fae1',
  deviceId: IOTDA_DEFAULTS.deviceId,
  deviceSecret: IOTDA_DEFAULTS.deviceSecret,
  iotdaAppHost: '8cee850016.st1.iotda-app.cn-north-4.myhuaweicloud.com',
  iotdaDeviceHost: IOTDA_DEFAULTS.deviceMqttHost,
};

export const defaultAppConfig: AppConfig = {
  baseUrl: IOTDA_DEFAULTS.baseUrl,
  projectId: IOTDA_DEFAULTS.projectId,
  deviceId: IOTDA_DEFAULTS.deviceId,
  ak: IOTDA_DEFAULTS.ak,
  sk: IOTDA_DEFAULTS.sk,
};

interface HttpResult<T> {
  ok: boolean;
  data: T;
  message: string;
}

interface IotdaCommandResponse {
  command_id?: string;
  error_code?: string;
  error_msg?: string;
  response?: {
    result_code?: number;
    response_name?: string;
    paras?: Record<string, unknown>;
  };
}

function normalizeBaseUrl(baseUrl: string): string {
  const trimmed = baseUrl.trim();
  if (trimmed.endsWith('/')) {
    return trimmed.slice(0, -1);
  }
  return trimmed;
}

function extractHost(baseUrl: string): string {
  const trimmed = normalizeBaseUrl(baseUrl);
  const withoutProto = trimmed.replace(/^https?:\/\//, '');
  const slashIdx = withoutProto.indexOf('/');
  return slashIdx >= 0 ? withoutProto.slice(0, slashIdx) : withoutProto;
}

function asObject(value: unknown): Record<string, unknown> | null {
  if (!value || typeof value !== 'object') {
    return null;
  }
  return value as Record<string, unknown>;
}

function toNumber(value: unknown, fallback: number): number {
  if (typeof value === 'number' && Number.isFinite(value)) {
    return value;
  }
  if (typeof value === 'string') {
    const parsed = Number(value);
    if (Number.isFinite(parsed)) {
      return parsed;
    }
  }
  return fallback;
}

function toBoolean(value: unknown, fallback: boolean): boolean {
  if (typeof value === 'boolean') {
    return value;
  }
  if (typeof value === 'string') {
    if (value.toLowerCase() === 'true') {
      return true;
    }
    if (value.toLowerCase() === 'false') {
      return false;
    }
  }
  return fallback;
}

function toString(value: unknown, fallback: string): string {
  if (typeof value === 'string') {
    return value;
  }
  return fallback;
}

function parseIsoTsMs(eventTime: string): number {
  const iotdaCompact = /^(\d{4})(\d{2})(\d{2})T(\d{2})(\d{2})(\d{2})Z$/;
  const matched = eventTime.match(iotdaCompact);
  if (matched !== null) {
    const normalized =
      `${matched[1]}-${matched[2]}-${matched[3]}T${matched[4]}:${matched[5]}:${matched[6]}Z`;
    const compactParsed = Date.parse(normalized);
    if (Number.isFinite(compactParsed)) {
      return compactParsed;
    }
  }
  const parsed = Date.parse(eventTime);
  if (Number.isFinite(parsed)) {
    return parsed;
  }
  return Date.now();
}

function formatHttpError(statusCode: number, bodyText: string): string {
  if (!bodyText) {
    return `HTTP_${statusCode}`;
  }
  try {
    const parsed = JSON.parse(bodyText) as Record<string, unknown>;
    const code = typeof parsed.error_code === 'string' ? parsed.error_code : '';
    const msg = typeof parsed.error_msg === 'string' ? parsed.error_msg : '';
    if (code || msg) {
      return `HTTP_${statusCode}_${code}_${msg}`;
    }
  } catch {
    // 忽略解析失败
  }
  return `HTTP_${statusCode}`;
}

function emptyLatest(): DeviceLatest {
  return {
    ts: 0,
    temp_c: 0,
    hum_rh: 0,
    lux: 0,
    food_g: 0,
    water_level_state: 'OK',
    voc_index: 0,
    tvoc_ppb: 0,
    fan_on: false,
    led_on: false,
    pump_on: false,
    servo_active: false,
    online: false,
    fw_version: '-',
    last_error: '',
  };
}

export class PetHomeApi {
  private readonly config: AppConfig;
  private readonly host: string;
  private readonly historyCache: Record<string, HistoryPoint[]> = {
    temp_c: [],
    hum_rh: [],
    lux: [],
    food_g: [],
    voc_index: [],
  };
  private readonly eventCache: DeviceEvent[] = [];
  private readonly commandCache: Array<{ ts: number; cmd: string }> = [];
  private lastLatest: DeviceLatest | null = null;

  constructor(config: AppConfig = defaultAppConfig) {
    this.config = {
      ...config,
      baseUrl: normalizeBaseUrl(config.baseUrl),
    };
    this.host = extractHost(this.config.baseUrl);
  }

  setDevice(deviceId: string): void {
    if (this.config.deviceId !== deviceId) {
      this.config.deviceId = deviceId;
      this.lastLatest = null;
      this.eventCache.length = 0;
      this.commandCache.length = 0;
      this.historyCache['temp_c'] = [];
      this.historyCache['hum_rh'] = [];
      this.historyCache['lux'] = [];
      this.historyCache['food_g'] = [];
      this.historyCache['voc_index'] = [];
    }
  }

  async getLatest(): Promise<HttpResult<DeviceLatest>> {
    const shadowPath = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/shadow`;
    const infoPath = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}`;

    const shadowResult = await this.requestIotdaJson<Record<string, unknown>>(
      'GET',
      shadowPath,
    );
    if (!shadowResult.ok) {
      return { ok: false, data: emptyLatest(), message: shadowResult.message };
    }

    const infoResult = await this.requestIotdaJson<Record<string, unknown>>(
      'GET',
      infoPath,
    );
    const latest = this.parseLatestFromShadow(
      shadowResult.data,
      infoResult.ok ? infoResult.data : undefined,
    );

    this.cacheHistory(latest);
    this.synthesizeEvents(latest);
    this.lastLatest = latest;
    return { ok: true, data: latest, message: 'OK' };
  }

  async getDeviceList(): Promise<HttpResult<string[]>> {
    const path = `/v5/iot/${this.config.projectId}/devices`;
    const result = await this.requestIotdaJson<Record<string, unknown>>('GET', path);
    if (!result.ok) {
      return { ok: false, data: [], message: result.message };
    }
    const devices = Array.isArray(result.data.devices)
      ? (result.data.devices as Array<Record<string, unknown>>)
      : [];
    const list: string[] = [];
    for (const d of devices) {
      if (typeof d.device_id === 'string' && d.device_id.length > 0) {
        list.push(d.device_id);
      }
    }
    return { ok: true, data: list, message: result.message };
  }

  async getEvents(): Promise<HttpResult<DeviceEvent[]>> {
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/messages?limit=50`;
    const cloudResult = await this.requestIotdaJson<Record<string, unknown>>('GET', path);
    if (cloudResult.ok) {
      const cloudEvents = this.parseCloudEvents(cloudResult.data);
      if (cloudEvents.length > 0) {
        return { ok: true, data: cloudEvents, message: '云端查询' };
      }
    }
    return { ok: true, data: [...this.eventCache], message: '本地缓存' };
  }

  async getHistory(metric: string, from?: number, to?: number, step?: number): Promise<HttpResult<HistoryPoint[]>> {
    // 云端优先：通过 IoTDA 消息接口获取设备历史上报属性
    let msgPath = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/messages?limit=200`;
    
    if (from !== undefined && to !== undefined) {
      const startStr = this.formatIotdaCompactTime(from);
      const endStr = this.formatIotdaCompactTime(to);
      msgPath += `&start_time=${startStr}&end_time=${endStr}`;
    }
    
    const cloudResult = await this.requestIotdaJson<Record<string, unknown>>('GET', msgPath);
    if (cloudResult.ok) {
      const cloudPoints = this.parseCloudHistory(cloudResult.data, metric);
      if (cloudPoints.length > 0) {
        // 云端数据合并到本地缓存
        for (const pt of cloudPoints) {
          this.pushMetric(metric, pt.ts, pt.value);
        }
      }
    }
    // 本地兜底
    const all = this.historyCache[metric] ?? [];
    
    // 如果指定了 from/to 则过滤范围，否则回退到过去 24 小时
    const actualFrom = from !== undefined ? from : Date.now() - 24 * 60 * 60 * 1000;
    const actualTo = to !== undefined ? to : Date.now();
    
    const data = all.filter((point) => point.ts >= actualFrom && point.ts <= actualTo);
    
    // 按时间戳去重排序
    const seen = new Set<number>();
    let unique: HistoryPoint[] = [];
    for (const pt of data) {
      const key = Math.floor(pt.ts / 1000);
      if (!seen.has(key)) {
        seen.add(key);
        unique.push(pt);
      }
    }
    unique.sort((a, b) => a.ts - b.ts);
    
    // 应用 step 粒度（例如：每 step 秒取一个点，避免曲线点过于密集）
    if (step !== undefined && step > 0 && unique.length > 1) {
      const sampled: HistoryPoint[] = [];
      let nextTs = unique[0].ts;
      const stepMs = step * 1000;
      for (const pt of unique) {
        if (pt.ts >= nextTs) {
          sampled.push(pt);
          nextTs = pt.ts + stepMs;
        }
      }
      unique = sampled;
    }

    return { ok: true, data: unique, message: unique.length > 0 ? '历史查询成功' : '暂无数据' };
  }

  private formatIotdaCompactTime(ts: number): string {
    const d = new Date(ts);
    const pad = (n: number) => n < 10 ? '0' + n : n.toString();
    const yyyy = d.getUTCFullYear();
    const MM = pad(d.getUTCMonth() + 1);
    const dd = pad(d.getUTCDate());
    const HH = pad(d.getUTCHours());
    const mm = pad(d.getUTCMinutes());
    const ss = pad(d.getUTCSeconds());
    return `${yyyy}${MM}${dd}T${HH}${mm}${ss}Z`;
  }

  async getDailyReport(date?: string): Promise<HttpResult<DailyReport>> {
    const targetDate = date ?? new Date().toISOString().slice(0, 10);
    const dayStart = new Date(`${targetDate}T00:00:00+08:00`).getTime();
    const dayEnd = dayStart + 24 * 60 * 60 * 1000;

    // 云端优先获取事件
    const eventsResult = await this.getEvents();
    const allEvents = eventsResult.ok ? eventsResult.data : this.eventCache;

    // 云端优先获取历史数据（触发云端查询写入缓存）
    await this.getHistory('temp_c');
    await this.getHistory('hum_rh');
    await this.getHistory('voc_index');

    const tempPoints = this.historyCache.temp_c.filter((p) => p.ts >= dayStart && p.ts < dayEnd);
    const humPoints = this.historyCache.hum_rh.filter((p) => p.ts >= dayStart && p.ts < dayEnd);
    const vocPoints = this.historyCache.voc_index.filter((p) => p.ts >= dayStart && p.ts < dayEnd);
    const events = allEvents.filter((e) => e.ts >= dayStart && e.ts < dayEnd);
    const commands = this.commandCache.filter((c) => c.ts >= dayStart && c.ts < dayEnd);

    const feedCount = commands.filter((item) => item.cmd === 'feed_once').length;
    const refillCount = events.filter((item) => item.event_type === 'LOW_WATER').length;
    const alertCount = events.length;

    const report: DailyReport = {
      date: targetDate,
      feed_count: feedCount,
      refill_count: refillCount,
      alert_count: alertCount,
      avg_temp_c: this.average(tempPoints),
      avg_hum_rh: this.average(humPoints),
      avg_voc_index: this.average(vocPoints),
      notes: alertCount > 0
        ? ['当天存在告警，请复核设备状态与云端记录。']
        : ['当天运行平稳，未检测到告警。'],
    };
    return { ok: true, data: report, message: '云端历史聚合' };
  }

  async sendActuateCommand(payload: Record<string, boolean>): Promise<HttpResult<CommandResponse>> {
    const paras: Record<string, unknown> = {};
    if (typeof payload.led_on === 'boolean') {
      paras.led_on = payload.led_on;
    }
    if (typeof payload.fan_on === 'boolean') {
      paras.fan_on = payload.fan_on;
    }
    if (typeof payload.pump_on === 'boolean') {
      paras.pump_on = payload.pump_on;
    }

    const body = {
      service_id: IOTDA_SERVICE_ID_CONTROL,
      command_name: IOTDA_COMMAND_NAME_CONTROL,
      paras,
    };
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands`;
    const result = await this.requestIotdaJson<IotdaCommandResponse>('POST', path, body);
    const mapped = this.mapCommandResponse(result, 'actuate');
    if (mapped.ok) {
      this.commandCache.unshift({ ts: mapped.data.ts, cmd: 'actuate' });
      this.trimCommandCache();
    }
    return mapped;
  }

  async sendFeedCommand(feedTarget: number): Promise<HttpResult<CommandResponse>> {
    const body = {
      service_id: IOTDA_SERVICE_ID_CONTROL,
      command_name: IOTDA_COMMAND_NAME_CONTROL,
      paras: {
        feed: true,
        feed_target_g: clampConfigValue('feed_target_g', feedTarget),
      },
    };
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands`;
    const result = await this.requestIotdaJson<IotdaCommandResponse>('POST', path, body);
    const mapped = this.mapCommandResponse(result, 'feed_once');
    if (mapped.ok) {
      this.commandCache.unshift({ ts: mapped.data.ts, cmd: 'feed_once' });
      this.trimCommandCache();
    }
    return mapped;
  }

  async sendSetConfig(configPayload: Record<string, number | boolean | string>): Promise<HttpResult<CommandResponse>> {
    const body = {
      service_id: IOTDA_SERVICE_ID_CONFIG,
      command_name: IOTDA_COMMAND_NAME_SET_CONFIG,
      paras: configPayload,
    };
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands`;
    const result = await this.requestIotdaJson<IotdaCommandResponse>('POST', path, body);
    const mapped = this.mapCommandResponse(result, 'set_config');
    if (mapped.ok) {
      this.commandCache.unshift({ ts: mapped.data.ts, cmd: 'set_config' });
      this.trimCommandCache();
    }
    return mapped;
  }

  async sendSetNightWindow(start: string, end: string): Promise<HttpResult<CommandResponse>> {
    const body = {
      service_id: IOTDA_SERVICE_ID_CONFIG,
      command_name: IOTDA_COMMAND_NAME_SET_NIGHT_WINDOW,
      paras: {
        night_mode_start: start.trim(),
        night_mode_end: end.trim(),
      },
    };
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands`;
    const result = await this.requestIotdaJson<IotdaCommandResponse>('POST', path, body);
    const mapped = this.mapCommandResponse(result, 'set_night_window');
    if (mapped.ok) {
      this.commandCache.unshift({ ts: mapped.data.ts, cmd: 'set_night_window' });
      this.trimCommandCache();
    }
    return mapped;
  }

  async getApStatus(apGateway: string): Promise<HttpResult<Record<string, string | boolean>>> {
    const base = normalizeApGateway(apGateway);
    return this.requestPlainJson<Record<string, string | boolean>>(
      `${base}/status`,
      http.RequestMethod.GET,
    );
  }

  async scanProvisionNetworks(apGateway: string): Promise<HttpResult<Record<string, string | number>[]>> {
    const base = normalizeApGateway(apGateway);
    const result = await this.requestPlainJson<{ networks: Record<string, string | number>[] }>(
      `${base}/scan`,
      http.RequestMethod.GET,
    );
    if (!result.ok) {
      return { ok: false, data: [], message: result.message };
    }
    return { ok: true, data: result.data.networks ?? [], message: 'OK' };
  }

  async submitProvisionConfig(apGateway: string, ssid: string, pwd: string): Promise<HttpResult<Record<string, string | boolean>>> {
    const base = normalizeApGateway(apGateway);
    const payload = buildProvisionConfigPayload(ssid, pwd);
    return this.requestPlainJson<Record<string, string | boolean>>(
      `${base}/config`,
      http.RequestMethod.POST,
      payload as Record<string, unknown>,
    );
  }

  private async requestIotdaJson<T>(
    method: 'GET' | 'POST',
    path: string,
    payload?: Record<string, unknown>,
  ): Promise<HttpResult<T>> {
    const url = `${this.config.baseUrl}${path}`;
    const body = payload ? JSON.stringify(payload) : '';
    const xSdkDate = buildXSdkDate(new Date());
    const contentType = payload ? 'application/json' : undefined;

    const authorization = await buildAuthorization({
      method,
      canonicalUri: path,
      canonicalQueryString: '',
      host: this.host,
      xSdkDate,
      contentType,
      body,
      ak: this.config.ak,
      sk: this.config.sk,
    });

    const headers: Record<string, string> = {
      'X-Sdk-Date': xSdkDate,
      Authorization: authorization,
      Accept: 'application/json',
    };
    if (contentType) {
      headers['Content-Type'] = contentType;
    }

    return this.executeHttpRequest<T>(
      url,
      method === 'GET' ? http.RequestMethod.GET : http.RequestMethod.POST,
      headers,
      body,
      15000,
    );
  }

  private async requestPlainJson<T>(
    url: string,
    method: http.RequestMethod,
    payload?: Record<string, unknown>,
  ): Promise<HttpResult<T>> {
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
    };
    const body = payload ? JSON.stringify(payload) : '';
    return this.executeHttpRequest<T>(url, method, headers, body, 10000);
  }

  private async executeHttpRequest<T>(
    url: string,
    method: http.RequestMethod,
    headers: Record<string, string>,
    body: string,
    timeoutMs: number,
  ): Promise<HttpResult<T>> {
    const client = http.createHttp();
    try {
      const options: http.HttpRequestOptions = {
        method,
        header: headers,
        connectTimeout: timeoutMs,
        readTimeout: timeoutMs,
      };
      if (body.length > 0 && method !== http.RequestMethod.GET) {
        options.extraData = body;
      }

      const response = await client.request(url, options);
      const statusCode = response.responseCode ?? 0;
      const bodyText =
        typeof response.result === 'string'
          ? response.result
          : JSON.stringify(response.result ?? {});

      if (statusCode < 200 || statusCode >= 300) {
        return { ok: false, data: {} as T, message: formatHttpError(statusCode, bodyText) };
      }

      if (bodyText.length === 0) {
        return { ok: true, data: {} as T, message: 'OK' };
      }
      return { ok: true, data: JSON.parse(bodyText) as T, message: 'OK' };
    } catch (err) {
      return { ok: false, data: {} as T, message: JSON.stringify(err) };
    } finally {
      client.destroy();
    }
  }

  private parseLatestFromShadow(
    shadowRoot: Record<string, unknown>,
    deviceInfo?: Record<string, unknown>,
  ): DeviceLatest {
    const latest = emptyLatest();
    const shadowArray = Array.isArray(shadowRoot.shadow)
      ? (shadowRoot.shadow as Array<Record<string, unknown>>)
      : [];

    let selectedProps: Record<string, unknown> = {};
    let eventTime = '';
    for (let i = 0; i < shadowArray.length; i++) {
      const item = asObject(shadowArray[i]);
      if (!item) {
        continue;
      }
      const reported = asObject(item.reported);
      const props = asObject(reported?.properties);
      if (!props) {
        continue;
      }

      const serviceId = toString(item.service_id, '').toLowerCase();
      selectedProps = props;
      eventTime = toString(reported?.event_time, '');
      if (serviceId.includes('pet')) {
        break;
      }
    }

    latest.temp_c = toNumber(
      selectedProps.temp_c ?? selectedProps.temperature ?? selectedProps.temp,
      0,
    );
    latest.hum_rh = toNumber(
      selectedProps.hum_rh ?? selectedProps.humidity ?? selectedProps.hum,
      0,
    );
    latest.lux = toNumber(
      selectedProps.lux ?? selectedProps.light ?? selectedProps.light_lux,
      0,
    );
    latest.food_g = toNumber(
      selectedProps.food_g ?? selectedProps.food_weight ?? selectedProps.food,
      0,
    );

    const waterState = selectedProps.water_level_state;
    if (typeof waterState === 'string') {
      latest.water_level_state = waterState.toUpperCase().includes('LOW') ? 'LOW' : 'OK';
    } else {
      const waterLevel = toNumber(selectedProps.water_level, 100);
      latest.water_level_state = waterLevel <= 20 ? 'LOW' : 'OK';
    }

    latest.voc_index = Math.round(
      toNumber(selectedProps.voc_index ?? selectedProps.voc, 0),
    );
    latest.tvoc_ppb = Math.round(
      toNumber(selectedProps.tvoc_ppb ?? selectedProps.tvoc, 0),
    );
    latest.fan_on = toBoolean(
      selectedProps.fan_on,
      false,
    );
    latest.led_on = toBoolean(
      selectedProps.led_on ?? selectedProps.led ?? selectedProps.light_on,
      false,
    );
    latest.pump_on = toBoolean(
      selectedProps.pump_on,
      false,
    );
    latest.servo_active = toBoolean(
      selectedProps.servo_active,
      false,
    );
    latest.fw_version = toString(
      selectedProps.fw_version ?? shadowRoot.firmware_version,
      '-',
    );
    latest.last_error = toString(selectedProps.last_error, '');
    latest.ts = eventTime.length > 0
      ? parseIsoTsMs(eventTime)
      : Math.round(toNumber(selectedProps.ts, Date.now()));
    latest.online = toString(deviceInfo?.status, '').toUpperCase() === 'ONLINE' ||
      toBoolean(selectedProps.online, false);
    return latest;
  }

  private cacheHistory(latest: DeviceLatest): void {
    this.pushMetric('temp_c', latest.ts, latest.temp_c);
    this.pushMetric('hum_rh', latest.ts, latest.hum_rh);
    this.pushMetric('lux', latest.ts, latest.lux);
    this.pushMetric('food_g', latest.ts, latest.food_g);
    this.pushMetric('voc_index', latest.ts, latest.voc_index);
  }

  private pushMetric(metric: string, ts: number, value: number): void {
    if (this.historyCache[metric] === undefined) {
      this.historyCache[metric] = [];
    }
    this.historyCache[metric].push({ ts, value });
    if (this.historyCache[metric].length > 600) {
      this.historyCache[metric] = this.historyCache[metric].slice(-600);
    }
  }

  private synthesizeEvents(latest: DeviceLatest): void {
    const prev = this.lastLatest;
    if (!prev) {
      return;
    }
    if (prev.water_level_state !== latest.water_level_state &&
      latest.water_level_state === 'LOW') {
      this.pushEvent({
        ts: latest.ts,
        event_type: 'LOW_WATER',
        level: 'WARN',
        detail: 'water_level_low',
      });
    }
    if (prev.food_g >= 50 && latest.food_g < 50) {
      this.pushEvent({
        ts: latest.ts,
        event_type: 'LOW_FOOD',
        level: 'WARN',
        detail: 'food_weight_low',
      });
    }
    if (prev.online && !latest.online) {
      this.pushEvent({
        ts: latest.ts,
        event_type: 'CLOUD_OFFLINE',
        level: 'ERROR',
        detail: 'device_offline',
      });
    }
    if (latest.last_error.length > 0 && latest.last_error !== prev.last_error) {
      this.pushEvent({
        ts: latest.ts,
        event_type: 'SENSOR_FAULT',
        level: 'WARN',
        detail: latest.last_error,
      });
    }
  }

  private pushEvent(event: DeviceEvent): void {
    this.eventCache.unshift(event);
    if (this.eventCache.length > 100) {
      this.eventCache.splice(100);
    }
  }

  private trimCommandCache(): void {
    if (this.commandCache.length > 100) {
      this.commandCache.splice(100);
    }
  }

  private average(points: HistoryPoint[]): number {
    if (points.length === 0) {
      return 0;
    }
    let sum = 0;
    for (let i = 0; i < points.length; i++) {
      sum += points[i].value;
    }
    return Math.round((sum / points.length) * 10) / 10;
  }

  private parseCloudEvents(data: Record<string, unknown>): DeviceEvent[] {
    const result: DeviceEvent[] = [];
    const messages = Array.isArray(data.device_messages)
      ? (data.device_messages as Array<Record<string, unknown>>)
      : (Array.isArray(data.messages) ? (data.messages as Array<Record<string, unknown>>) : []);
    for (let i = 0; i < messages.length && i < 50; i++) {
      const msg = asObject(messages[i]);
      if (!msg) continue;
      try {
        const content = typeof msg.message === 'string' ? JSON.parse(msg.message) as Record<string, unknown> : asObject(msg.message);
        if (!content) continue;
        const services = Array.isArray(content.services) ? (content.services as Array<Record<string, unknown>>) : [];
        for (const svc of services) {
          const eventType = toString(svc.event_type, '');
          if (eventType.length === 0) continue;
          const paras = asObject(svc.paras) ?? {};
          result.push({
            ts: typeof msg.timestamp === 'string' ? parseIsoTsMs(msg.timestamp as string) : toNumber(msg.timestamp, Date.now()),
            event_type: eventType,
            level: toString(paras.level, 'INFO'),
            detail: toString(paras.detail, ''),
          });
        }
      } catch {
        // 忽略解析失败的消息
      }
    }
    return result;
  }

  private parseCloudHistory(data: Record<string, unknown>, metric: string): HistoryPoint[] {
    const result: HistoryPoint[] = [];
    const messages = Array.isArray(data.device_messages)
      ? (data.device_messages as Array<Record<string, unknown>>)
      : (Array.isArray(data.messages) ? (data.messages as Array<Record<string, unknown>>) : []);
    for (let i = 0; i < messages.length && i < 200; i++) {
      const msg = asObject(messages[i]);
      if (!msg) continue;
      try {
        const content = typeof msg.message === 'string'
          ? JSON.parse(msg.message) as Record<string, unknown>
          : asObject(msg.message);
        if (!content) continue;
        const services = Array.isArray(content.services)
          ? (content.services as Array<Record<string, unknown>>)
          : [];
        for (const svc of services) {
          const props = asObject(svc.properties);
          if (!props) continue;
          const val = props[metric];
          if (val === undefined || val === null) continue;
          const numVal = toNumber(val, NaN);
          if (!Number.isFinite(numVal)) continue;
          // 从消息时间戳或 event_time 中提取时间
          let ts = Date.now();
          const eventTime = toString(svc.event_time, '');
          if (eventTime.length > 0) {
            ts = parseIsoTsMs(eventTime);
          } else if (typeof msg.timestamp === 'string') {
            ts = parseIsoTsMs(msg.timestamp as string);
          } else {
            ts = toNumber(msg.timestamp, Date.now());
          }
          result.push({ ts, value: numVal });
        }
      } catch {
        // 忽略解析失败的消息
      }
    }
    return result;
  }

  private mapCommandResponse(
    result: HttpResult<IotdaCommandResponse>,
    cmd: string,
  ): HttpResult<CommandResponse> {
    if (!result.ok) {
      return {
        ok: false,
        data: {
          request_id: `req_${Date.now()}`,
          result: 'FAILED',
          reason: result.message,
          ts: Date.now(),
        },
        message: result.message,
      };
    }

    const raw = result.data;
    if (typeof raw.error_code === 'string' && raw.error_code.length > 0) {
      return {
        ok: false,
        data: {
          request_id: toString(raw.command_id, `req_${Date.now()}`),
          result: 'FAILED',
          reason: `${raw.error_code}:${toString(raw.error_msg, 'IOTDA_ERROR')}`,
          ts: Date.now(),
        },
        message: toString(raw.error_msg, raw.error_code),
      };
    }

    const resultCode = toNumber(raw.response?.result_code, 0);
    let mappedResult: 'OK' | 'REJECTED' | 'FAILED' = 'FAILED';
    if (resultCode === 0) {
      mappedResult = 'OK';
    } else if (resultCode === 1) {
      mappedResult = 'REJECTED';
    } else {
      mappedResult = 'FAILED';
    }

    const commandResult: CommandResponse = {
      request_id: toString(raw.command_id, `req_${Date.now()}`),
      result: mappedResult,
      reason: toString(raw.response?.paras?.reason, toString(raw.response?.response_name, `${cmd}_accepted`)),
      ts: Date.now(),
    };
    // OK 和 REJECTED 都视为“已送达”，仅 FAILED 视为通信失败
    return {
      ok: mappedResult === 'OK' || mappedResult === 'REJECTED',
      data: commandResult,
      message: mappedResult === 'OK' ? '执行成功' : (mappedResult === 'REJECTED' ? '设备已拒绝' : commandResult.reason),
    };
  }
}
