import http from '@ohos.net.http';
import { AppConfig, CommandResponse, DailyReport, DeviceEvent, DeviceLatest, HistoryPoint } from '../model/PetHomeModelsTs';
import { ProvisionConfigPayload, buildProvisionConfigPayload, clampConfigValue, normalizeApGateway } from '../utils/PetHomeValidatorsTs';
import { IOTDA_COMMAND_NAME_CONTROL, IOTDA_COMMAND_NAME_SET_CONFIG, IOTDA_COMMAND_NAME_SET_NIGHT_WINDOW, IOTDA_DEFAULTS, IOTDA_SERVICE_ID_CONFIG, IOTDA_SERVICE_ID_CONTROL } from '../config/iotdaDefaultsTs';
import { buildAuthorization, buildXSdkDate } from '../api/iotdaSignerTs';
import { IotdaCommandResponse, mapIotdaCommandQueryResponse, mapIotdaCommandResponse } from './CommandResponseMapperTs';
type JsonPrimitive = string | number | boolean | null;
type JsonValue = JsonPrimitive | JsonObject | JsonObject[] | string[] | number[] | boolean[];
interface JsonObject { [key: string]: JsonValue; }
interface HttpResult<T> { ok: boolean; data: T; message: string; }
interface HuaweiCloudFixedConfig { region: string; instanceId: string; projectId: string; iamAk: string; iamSk: string; appId: string; productId: string; deviceId: string; deviceSecret: string; iotdaAppHost: string; iotdaDeviceHost: string; }
interface HistoryCache { temp_c: HistoryPoint[]; hum_rh: HistoryPoint[]; lux: HistoryPoint[]; food_g: HistoryPoint[]; voc_index: HistoryPoint[]; }
interface CommandCacheItem { ts: number; cmd: string; }
export interface ActuateCommandPayload { led_on?: boolean; fan_on?: boolean; pump_on?: boolean; }
export interface SetConfigPayload { [key: string]: string | number | boolean; }
export interface ApStatusData { [key: string]: string | boolean; }
export interface ApNetworkItem { [key: string]: string | number; }
interface ScanNetworkResponse { networks: ApNetworkItem[]; }
interface IotdaCommandRequestBody { service_id: string; command_name: string; paras: JsonObject; }
interface PathWithQuery { canonicalUri: string; canonicalQueryString: string; }
interface HttpHeaderMap { [key: string]: string; }
export const huaweiCloudFixedConfig: HuaweiCloudFixedConfig = {
  region: IOTDA_DEFAULTS.region,
  instanceId: IOTDA_DEFAULTS.instanceId,
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
  region: IOTDA_DEFAULTS.region,
  instanceId: IOTDA_DEFAULTS.instanceId,
  projectId: IOTDA_DEFAULTS.projectId,
  deviceId: IOTDA_DEFAULTS.deviceId,
  ak: IOTDA_DEFAULTS.ak,
  sk: IOTDA_DEFAULTS.sk,
};
function createJsonObject(): JsonObject {
  return JSON.parse('{}') as JsonObject;
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
function splitPathAndQuery(path: string): PathWithQuery {
  const idx = path.indexOf('?');
  if (idx < 0) {
    return {
      canonicalUri: path,
      canonicalQueryString: '',
    };
  }
  const canonicalUri = path.slice(0, idx);
  const queryRaw = path.slice(idx + 1);
  if (queryRaw.length === 0) {
    return {
      canonicalUri,
      canonicalQueryString: '',
    };
  }
  const queryItems = queryRaw.split('&').filter((item) => item.length > 0);
  queryItems.sort((a, b) => {
    if (a === b) {
      return 0;
    }
    return a < b ? -1 : 1;
  });
  return {
    canonicalUri,
    canonicalQueryString: queryItems.join('&'),
  };
}
function asJsonObject(value: JsonValue | undefined): JsonObject | null {
  if (value === undefined || value === null) {
    return null;
  }
  if (Array.isArray(value)) {
    return null;
  }
  if (typeof value === 'object') {
    return value as JsonObject;
  }
  return null;
}
function asJsonObjectArray(value: JsonValue | undefined): JsonObject[] {
  if (!Array.isArray(value)) {
    return [];
  }
  const arr = value as JsonValue[];
  const result: JsonObject[] = [];
  for (let i = 0; i < arr.length; i++) {
    const item = asJsonObject(arr[i]);
    if (item !== null) {
      result.push(item);
    }
  }
  return result;
}
function toNumber(value: JsonValue | undefined, fallback: number): number {
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
function toBoolean(value: JsonValue | undefined, fallback: boolean): boolean {
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
function toStringValue(value: JsonValue | undefined, fallback: string): string {
  if (typeof value === 'string') {
    return value;
  }
  return fallback;
}
function parseIsoTsMs(eventTime: string): number {
  const iotdaCompact = /^(\d{4})(\d{2})(\d{2})T(\d{2})(\d{2})(\d{2})Z$/;
  const matched = eventTime.match(iotdaCompact);
  if (matched !== null) {
    const normalized = `${matched[1]}-${matched[2]}-${matched[3]}T${matched[4]}:${matched[5]}:${matched[6]}Z`;
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
function parseJsonObject(text: string): JsonObject | null {
  if (text.length === 0) {
    return null;
  }
  try {
    const parsed = JSON.parse(text) as JsonValue;
    return asJsonObject(parsed);
  } catch {
    return null;
  }
}
function formatHttpError(statusCode: number, bodyText: string): string {
  if (bodyText.length === 0) {
    return `HTTP_${statusCode}`;
  }
  const parsed = parseJsonObject(bodyText);
  if (parsed !== null) {
    const code = toStringValue(parsed['error_code'], '');
    const msg = toStringValue(parsed['error_msg'], '');
    if (code.length > 0 || msg.length > 0) {
      return `HTTP_${statusCode}_${code}_${msg}`;
    }
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
const kCommandTrackTimeoutMs = 12000;
const kCommandTrackIntervalMs = 1500;
export class PetHomeApi {
  private readonly config: AppConfig;
  private readonly host: string;
  private readonly historyCache: HistoryCache = {
    temp_c: [],
    hum_rh: [],
    lux: [],
    food_g: [],
    voc_index: [],
  };
  private readonly eventCache: DeviceEvent[] = [];
  private readonly commandCache: CommandCacheItem[] = [];
  private lastLatest: DeviceLatest | null = null;
  constructor(config: AppConfig = defaultAppConfig) {
    this.config = {
      baseUrl: normalizeBaseUrl(config.baseUrl),
      region: config.region,
      instanceId: config.instanceId,
      projectId: config.projectId,
      deviceId: config.deviceId,
      ak: config.ak,
      sk: config.sk,
    };
    this.host = extractHost(this.config.baseUrl);
  }
  setDevice(deviceId: string): void {
    if (this.config.deviceId !== deviceId) {
      this.config.deviceId = deviceId;
      this.lastLatest = null;
      this.eventCache.length = 0;
      this.commandCache.length = 0;
      this.historyCache.temp_c.length = 0;
      this.historyCache.hum_rh.length = 0;
      this.historyCache.lux.length = 0;
      this.historyCache.food_g.length = 0;
      this.historyCache.voc_index.length = 0;
    }
  }
  async getLatest(): Promise<HttpResult<DeviceLatest>> {
    const shadowPath = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/shadow`;
    const infoPath = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}`;
    const shadowResult = await this.requestIotdaJson<JsonObject>('GET', shadowPath);
    if (!shadowResult.ok) {
      return {
        ok: false,
        data: emptyLatest(),
        message: shadowResult.message,
      };
    }
    const infoResult = await this.requestIotdaJson<JsonObject>('GET', infoPath);
    const latest = this.parseLatestFromShadow(
      shadowResult.data,
      infoResult.ok ? infoResult.data : undefined,
    );
    this.cacheHistory(latest);
    this.synthesizeEvents(latest);
    this.lastLatest = latest;
    return {
      ok: true,
      data: latest,
      message: 'OK',
    };
  }
  async getDeviceList(): Promise<HttpResult<string[]>> {
    const path = `/v5/iot/${this.config.projectId}/devices`;
    const result = await this.requestIotdaJson<JsonObject>('GET', path);
    if (!result.ok) {
      return {
        ok: false,
        data: [],
        message: result.message,
      };
    }
    const devices = asJsonObjectArray(result.data['devices']);
    const list: string[] = [];
    for (let i = 0; i < devices.length; i++) {
      const deviceId = toStringValue(devices[i]['device_id'], '');
      if (deviceId.length > 0) {
        list.push(deviceId);
      }
    }
    return {
      ok: true,
      data: list,
      message: result.message,
    };
  }
  async getEvents(): Promise<HttpResult<DeviceEvent[]>> {
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/messages?limit=50`;
    const cloudResult = await this.requestIotdaJson<JsonObject>('GET', path);
    if (cloudResult.ok) {
      const cloudEvents = this.parseCloudEvents(cloudResult.data);
      if (cloudEvents.length > 0) {
        return {
          ok: true,
          data: cloudEvents,
          message: '云端查询',
        };
      }
    }
    return {
      ok: true,
      data: this.copyEvents(),
      message: '本地缓存',
    };
  }
  async getHistory(
    metric: string,
    from?: number,
    to?: number,
    step?: number,
  ): Promise<HttpResult<HistoryPoint[]>> {
    let msgPath = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/messages?limit=200`;
    if (from !== undefined && to !== undefined) {
      const startStr = this.formatIotdaCompactTime(from);
      const endStr = this.formatIotdaCompactTime(to);
      msgPath += `&start_time=${startStr}&end_time=${endStr}`;
    }
    const cloudResult = await this.requestIotdaJson<JsonObject>('GET', msgPath);
    if (cloudResult.ok) {
      const cloudPoints = this.parseCloudHistory(cloudResult.data, metric);
      for (let i = 0; i < cloudPoints.length; i++) {
        this.pushMetric(metric, cloudPoints[i].ts, cloudPoints[i].value);
      }
    }
    const all = this.getMetricSeries(metric);
    const actualFrom = from !== undefined ? from : Date.now() - 24 * 60 * 60 * 1000;
    const actualTo = to !== undefined ? to : Date.now();
    const data = all.filter((point) => point.ts >= actualFrom && point.ts <= actualTo);
    const seen = new Set<number>();
    let unique: HistoryPoint[] = [];
    for (let i = 0; i < data.length; i++) {
      const key = Math.floor(data[i].ts / 1000);
      if (!seen.has(key)) {
        seen.add(key);
        unique.push(data[i]);
      }
    }
    unique.sort((a, b) => a.ts - b.ts);
    if (step !== undefined && step > 0 && unique.length > 1) {
      const sampled: HistoryPoint[] = [];
      const stepMs = step * 1000;
      let nextTs = unique[0].ts;
      for (const point of unique) {
        if (point.ts >= nextTs) {
          sampled.push(point);
          nextTs = point.ts + stepMs;
        }
      }
      unique = sampled;
    }
    return {
      ok: true,
      data: unique,
      message: unique.length > 0 ? '历史查询成功' : '暂无数据',
    };
  }
  private formatIotdaCompactTime(ts: number): string {
    const d = new Date(ts);
    const pad = (n: number) => n < 10 ? `0${n}` : `${n}`;
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
    const eventsResult = await this.getEvents();
    const allEvents = eventsResult.ok ? eventsResult.data : this.copyEvents();
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
    return {
      ok: true,
      data: report,
      message: '云端历史聚合',
    };
  }
  async sendActuateCommand(payload: ActuateCommandPayload): Promise<HttpResult<CommandResponse>> {
    const paras = createJsonObject();
    if (payload.led_on !== undefined) {
      paras['led_on'] = payload.led_on;
    }
    if (payload.fan_on !== undefined) {
      paras['fan_on'] = payload.fan_on;
    }
    if (payload.pump_on !== undefined) {
      paras['pump_on'] = payload.pump_on;
    }
    const body = this.buildCommandBody(IOTDA_SERVICE_ID_CONTROL, IOTDA_COMMAND_NAME_CONTROL, paras);
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands`;
    const result = await this.requestIotdaJson<IotdaCommandResponse>('POST', path, body);
    const mapped = mapIotdaCommandResponse(result, 'actuate');
    const settled = await this.trackAcceptedCommand(mapped, 'actuate');
    if (settled.ok) {
      this.commandCache.unshift({ ts: settled.data.ts, cmd: 'actuate' });
      this.trimCommandCache();
    }
    return settled;
  }
  async sendFeedCommand(feedTarget: number): Promise<HttpResult<CommandResponse>> {
    const paras = createJsonObject();
    paras['feed'] = true;
    paras['feed_target_g'] = clampConfigValue('feed_target_g', feedTarget);
    const body = this.buildCommandBody(IOTDA_SERVICE_ID_CONTROL, IOTDA_COMMAND_NAME_CONTROL, paras);
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands`;
    const result = await this.requestIotdaJson<IotdaCommandResponse>('POST', path, body);
    const mapped = mapIotdaCommandResponse(result, 'feed_once');
    const settled = await this.trackAcceptedCommand(mapped, 'feed_once');
    if (settled.ok) {
      this.commandCache.unshift({ ts: settled.data.ts, cmd: 'feed_once' });
      this.trimCommandCache();
    }
    return settled;
  }
  async sendSetConfig(configPayload: SetConfigPayload): Promise<HttpResult<CommandResponse>> {
    const paras = createJsonObject();
    for (const key in configPayload) {
      paras[key] = configPayload[key];
    }
    const body = this.buildCommandBody(IOTDA_SERVICE_ID_CONFIG, IOTDA_COMMAND_NAME_SET_CONFIG, paras);
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands`;
    const result = await this.requestIotdaJson<IotdaCommandResponse>('POST', path, body);
    const mapped = mapIotdaCommandResponse(result, 'set_config');
    const settled = await this.trackAcceptedCommand(mapped, 'set_config');
    if (settled.ok) {
      this.commandCache.unshift({ ts: settled.data.ts, cmd: 'set_config' });
      this.trimCommandCache();
    }
    return settled;
  }
  async sendSetNightWindow(start: string, end: string): Promise<HttpResult<CommandResponse>> {
    const paras = createJsonObject();
    paras['night_mode_start'] = start.trim();
    paras['night_mode_end'] = end.trim();
    const body = this.buildCommandBody(IOTDA_SERVICE_ID_CONFIG, IOTDA_COMMAND_NAME_SET_NIGHT_WINDOW, paras);
    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands`;
    const result = await this.requestIotdaJson<IotdaCommandResponse>('POST', path, body);
    const mapped = mapIotdaCommandResponse(result, 'set_night_window');
    const settled = await this.trackAcceptedCommand(mapped, 'set_night_window');
    if (settled.ok) {
      this.commandCache.unshift({ ts: settled.data.ts, cmd: 'set_night_window' });
      this.trimCommandCache();
    }
    return settled;
  }
  async getApStatus(apGateway: string): Promise<HttpResult<ApStatusData>> {
    const base = normalizeApGateway(apGateway);
    return this.requestPlainJson<ApStatusData>(
      `${base}/status`,
      http.RequestMethod.GET,
    );
  }
  async scanProvisionNetworks(apGateway: string): Promise<HttpResult<ApNetworkItem[]>> {
    const base = normalizeApGateway(apGateway);
    const result = await this.requestPlainJson<ScanNetworkResponse>(
      `${base}/scan`,
      http.RequestMethod.GET,
    );
    if (!result.ok) {
      return {
        ok: false,
        data: [],
        message: result.message,
      };
    }
    const list: ApNetworkItem[] = [];
    for (let i = 0; i < result.data.networks.length; i++) {
      list.push(result.data.networks[i]);
    }
    return {
      ok: true,
      data: list,
      message: 'OK',
    };
  }
  async submitProvisionConfig(
    apGateway: string,
    ssid: string,
    pwd: string,
  ): Promise<HttpResult<ApStatusData>> {
    const base = normalizeApGateway(apGateway);
    const payload = buildProvisionConfigPayload(ssid, pwd);
    const jsonPayload = this.provisionPayloadToJson(payload);
    return this.requestPlainJson<ApStatusData>(
      `${base}/config`,
      http.RequestMethod.POST,
      jsonPayload,
    );
  }
  private buildCommandBody(serviceId: string, commandName: string, paras: JsonObject): JsonObject {
    const bodyTyped: IotdaCommandRequestBody = {
      service_id: serviceId,
      command_name: commandName,
      paras,
    };
    const jsonBody = createJsonObject();
    jsonBody['service_id'] = bodyTyped.service_id;
    jsonBody['command_name'] = bodyTyped.command_name;
    jsonBody['paras'] = bodyTyped.paras;
    return jsonBody;
  }
  private provisionPayloadToJson(payload: ProvisionConfigPayload): JsonObject {
    const jsonPayload = createJsonObject();
    jsonPayload['ssid'] = payload.ssid;
    jsonPayload['pwd'] = payload.pwd;
    return jsonPayload;
  }
  private async requestIotdaJson<T>(
    method: 'GET' | 'POST',
    path: string,
    payload?: JsonObject,
  ): Promise<HttpResult<T>> {
    const url = `${this.config.baseUrl}${path}`;
    const bodyText = payload ? JSON.stringify(payload) : '';
    const xSdkDate = buildXSdkDate(new Date());
    const contentType = payload ? 'application/json' : '';
    const canonical = splitPathAndQuery(path);
    const authorization = await buildAuthorization({
      method,
      canonicalUri: canonical.canonicalUri,
      canonicalQueryString: canonical.canonicalQueryString,
      region: this.config.region,
      instanceId: this.config.instanceId,
      host: this.host,
      xSdkDate,
      contentType: contentType.length > 0 ? contentType : undefined,
      body: bodyText,
      ak: this.config.ak,
      sk: this.config.sk,
    });
    const headers = this.buildIotdaHeaders(xSdkDate, authorization, contentType);
    return this.executeHttpRequest<T>(
      url,
      method === 'GET' ? http.RequestMethod.GET : http.RequestMethod.POST,
      headers,
      bodyText,
      15000,
    );
  }
  private async requestPlainJson<T>(
    url: string,
    method: http.RequestMethod,
    payload?: JsonObject,
  ): Promise<HttpResult<T>> {
    const headers = this.buildPlainHeaders();
    const body = payload ? JSON.stringify(payload) : '';
    return this.executeHttpRequest<T>(url, method, headers, body, 10000);
  }
  private buildIotdaHeaders(xSdkDate: string, authorization: string, contentType: string): HttpHeaderMap {
    const headers: HttpHeaderMap = {};
    headers['X-Sdk-Date'] = xSdkDate;
    headers['Instance-Id'] = this.config.instanceId;
    headers['Authorization'] = authorization;
    headers['Accept'] = 'application/json';
    if (contentType.length > 0) {
      headers['Content-Type'] = contentType;
    }
    return headers;
  }
  private buildPlainHeaders(): HttpHeaderMap {
    const headers: HttpHeaderMap = {};
    headers['Content-Type'] = 'application/json';
    return headers;
  }
  private async executeHttpRequest<T>(
    url: string,
    method: http.RequestMethod,
    headers: HttpHeaderMap,
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
      let bodyText = '';
      if (typeof response.result === 'string') {
        bodyText = response.result;
      } else if (response.result !== undefined && response.result !== null) {
        bodyText = JSON.stringify(response.result);
      }
      if (statusCode < 200 || statusCode >= 300) {
        return {
          ok: false,
          data: this.emptyTypedValue<T>(),
          message: formatHttpError(statusCode, bodyText),
        };
      }
      if (bodyText.length === 0) {
        return {
          ok: true,
          data: this.emptyTypedValue<T>(),
          message: 'OK',
        };
      }
      const parsed = JSON.parse(bodyText) as T;
      return {
        ok: true,
        data: parsed,
        message: 'OK',
      };
    } catch (err) {
      return {
        ok: false,
        data: this.emptyTypedValue<T>(),
        message: JSON.stringify(err),
      };
    } finally {
      client.destroy();
    }
  }
  private async trackAcceptedCommand(
    mapped: HttpResult<CommandResponse>,
    cmd: string,
  ): Promise<HttpResult<CommandResponse>> {
    if (!mapped.ok || mapped.data.result !== 'ACCEPTED') {
      return mapped;
    }

    const commandId = mapped.data.request_id.trim();
    if (commandId.length === 0) {
      return mapped;
    }

    const path = `/v5/iot/${this.config.projectId}/devices/${this.config.deviceId}/commands/${commandId}`;
    const startMs = Date.now();
    while ((Date.now() - startMs) < kCommandTrackTimeoutMs) {
      const query = await this.requestIotdaJson<IotdaCommandResponse>('GET', path);
      if (query.ok) {
        const settled = mapIotdaCommandQueryResponse(query, cmd, commandId);
        if (settled.data.result !== 'ACCEPTED') {
          return settled;
        }
      }
      await this.sleepMs(kCommandTrackIntervalMs);
    }

    return {
      ok: true,
      data: {
        request_id: commandId,
        result: 'ACCEPTED',
        reason: 'COMMAND_TRACK_TIMEOUT',
        ts: Date.now(),
      },
      message: '云端已受理，设备执行中（追踪超时，可稍后刷新）',
    };
  }
  private async sleepMs(durationMs: number): Promise<void> {
    await new Promise<void>((resolve) => {
      setTimeout(() => resolve(), durationMs);
    });
  }
  private emptyTypedValue<T>(): T {
    return JSON.parse('{}') as T;
  }
  private parseLatestFromShadow(shadowRoot: JsonObject, deviceInfo?: JsonObject): DeviceLatest {
    const latest = emptyLatest();
    const shadowArray = asJsonObjectArray(shadowRoot['shadow']);
    let selectedProps: JsonObject = createJsonObject();
    let eventTime = '';
    for (let i = 0; i < shadowArray.length; i++) {
      const item = shadowArray[i];
      const reported = asJsonObject(item['reported']);
      if (reported === null) {
        continue;
      }
      const props = asJsonObject(reported['properties']);
      if (props === null) {
        continue;
      }
      selectedProps = props;
      eventTime = toStringValue(reported['event_time'], '');
      const serviceId = toStringValue(item['service_id'], '').toLowerCase();
      if (serviceId.includes('pet')) {
        break;
      }
    }
    latest.temp_c = toNumber(
      selectedProps['temp_c'] ?? selectedProps['temperature'] ?? selectedProps['temp'],
      0,
    );
    latest.hum_rh = toNumber(
      selectedProps['hum_rh'] ?? selectedProps['humidity'] ?? selectedProps['hum'],
      0,
    );
    latest.lux = toNumber(
      selectedProps['lux'] ?? selectedProps['light'] ?? selectedProps['light_lux'],
      0,
    );
    latest.food_g = toNumber(
      selectedProps['food_g'] ?? selectedProps['food_weight'] ?? selectedProps['food'],
      0,
    );
    const waterState = selectedProps['water_level_state'];
    if (typeof waterState === 'string') {
      latest.water_level_state = waterState.toUpperCase().includes('LOW') ? 'LOW' : 'OK';
    } else {
      const waterLevel = toNumber(selectedProps['water_level'], 100);
      latest.water_level_state = waterLevel <= 20 ? 'LOW' : 'OK';
    }
    latest.voc_index = Math.round(toNumber(selectedProps['voc_index'] ?? selectedProps['voc'], 0));
    latest.tvoc_ppb = Math.round(toNumber(selectedProps['tvoc_ppb'] ?? selectedProps['tvoc'], 0));
    latest.fan_on = toBoolean(selectedProps['fan_on'], false);
    latest.led_on = toBoolean(
      selectedProps['led_on'] ?? selectedProps['led'] ?? selectedProps['light_on'],
      false,
    );
    latest.pump_on = toBoolean(selectedProps['pump_on'], false);
    latest.servo_active = toBoolean(selectedProps['servo_active'], false);
    latest.fw_version = toStringValue(
      selectedProps['fw_version'] ?? shadowRoot['firmware_version'],
      '-',
    );
    latest.last_error = toStringValue(selectedProps['last_error'], '');
    latest.ts = eventTime.length > 0
      ? parseIsoTsMs(eventTime)
      : Math.round(toNumber(selectedProps['ts'], Date.now()));
    const infoStatus = deviceInfo ? toStringValue(deviceInfo['status'], '').toUpperCase() : '';
    if (infoStatus.length > 0) {
      latest.online = infoStatus === 'ONLINE';
    } else {
      latest.online = toBoolean(selectedProps['online'], false);
    }
    return latest;
  }
  private cacheHistory(latest: DeviceLatest): void {
    this.pushMetric('temp_c', latest.ts, latest.temp_c);
    this.pushMetric('hum_rh', latest.ts, latest.hum_rh);
    this.pushMetric('lux', latest.ts, latest.lux);
    this.pushMetric('food_g', latest.ts, latest.food_g);
    this.pushMetric('voc_index', latest.ts, latest.voc_index);
  }
  private getMetricSeries(metric: string): HistoryPoint[] {
    if (metric === 'hum_rh') {
      return this.historyCache.hum_rh;
    }
    if (metric === 'lux') {
      return this.historyCache.lux;
    }
    if (metric === 'food_g') {
      return this.historyCache.food_g;
    }
    if (metric === 'voc_index') {
      return this.historyCache.voc_index;
    }
    return this.historyCache.temp_c;
  }
  private pushMetric(metric: string, ts: number, value: number): void {
    const series = this.getMetricSeries(metric);
    series.push({ ts, value });
    if (series.length > 600) {
      series.splice(0, series.length - 600);
    }
  }
  private synthesizeEvents(latest: DeviceLatest): void {
    const prev = this.lastLatest;
    if (prev === null) {
      return;
    }
    if (prev.water_level_state !== latest.water_level_state && latest.water_level_state === 'LOW') {
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
  private copyEvents(): DeviceEvent[] {
    const copied: DeviceEvent[] = [];
    for (let i = 0; i < this.eventCache.length; i++) {
      copied.push(this.eventCache[i]);
    }
    return copied;
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
  private normalizeEventLevel(level: string): 'INFO' | 'WARN' | 'ERROR' {
    if (level === 'ERROR') {
      return 'ERROR';
    }
    if (level === 'WARN') {
      return 'WARN';
    }
    return 'INFO';
  }
  private parseCloudEvents(data: JsonObject): DeviceEvent[] {
    const result: DeviceEvent[] = [];
    const primary = asJsonObjectArray(data['device_messages']);
    const messages = primary.length > 0 ? primary : asJsonObjectArray(data['messages']);
    for (let i = 0; i < messages.length && i < 50; i++) {
      const msg = messages[i];
      let content: JsonObject | null = null;
      const messageField = msg['message'];
      if (typeof messageField === 'string') {
        content = parseJsonObject(messageField);
      } else {
        content = asJsonObject(messageField);
      }
      if (content === null) {
        continue;
      }
      const services = asJsonObjectArray(content['services']);
      for (let j = 0; j < services.length; j++) {
        const svc = services[j];
        const eventType = toStringValue(svc['event_type'], '');
        if (eventType.length === 0) {
          continue;
        }
        const paras = asJsonObject(svc['paras']);
        const levelRaw = paras ? toStringValue(paras['level'], 'INFO') : 'INFO';
        const detailRaw = paras ? toStringValue(paras['detail'], '') : '';
        const tsValue = msg['timestamp'];
        const ts = typeof tsValue === 'string' ? parseIsoTsMs(tsValue) : toNumber(tsValue, Date.now());
        result.push({
          ts,
          event_type: eventType,
          level: this.normalizeEventLevel(levelRaw),
          detail: detailRaw,
        });
      }
    }
    return result;
  }
  private parseCloudHistory(data: JsonObject, metric: string): HistoryPoint[] {
    const result: HistoryPoint[] = [];
    const primary = asJsonObjectArray(data['device_messages']);
    const messages = primary.length > 0 ? primary : asJsonObjectArray(data['messages']);
    for (let i = 0; i < messages.length && i < 200; i++) {
      const msg = messages[i];
      let content: JsonObject | null = null;
      const messageField = msg['message'];
      if (typeof messageField === 'string') {
        content = parseJsonObject(messageField);
      } else {
        content = asJsonObject(messageField);
      }
      if (content === null) {
        continue;
      }
      const services = asJsonObjectArray(content['services']);
      for (let j = 0; j < services.length; j++) {
        const svc = services[j];
        const props = asJsonObject(svc['properties']);
        if (props === null) {
          continue;
        }
        const val = props[metric];
        if (val === undefined || val === null) {
          continue;
        }
        const numVal = toNumber(val, Number.NaN);
        if (!Number.isFinite(numVal)) {
          continue;
        }
        let ts = Date.now();
        const eventTime = toStringValue(svc['event_time'], '');
        if (eventTime.length > 0) {
          ts = parseIsoTsMs(eventTime);
        } else {
          const tsValue = msg['timestamp'];
          if (typeof tsValue === 'string') {
            ts = parseIsoTsMs(tsValue);
          } else {
            ts = toNumber(tsValue, Date.now());
          }
        }
        result.push({
          ts,
          value: numVal,
        });
      }
    }
    return result;
  }
}
