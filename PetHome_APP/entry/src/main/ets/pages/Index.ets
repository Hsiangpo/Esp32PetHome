import { DailyReport, DeviceEvent, DeviceLatest, HistoryPoint } from '../model/PetHomeModelsTs';
import { ActuateCommandPayload, PetHomeApi, SetConfigPayload } from '../service/PetHomeApi';
import { clampConfigValue, isValidTimeValue, normalizeTimeValue } from '../utils/PetHomeValidatorsTs';
import dataPreferences from '@ohos.data.preferences';

const DEFAULT_LATEST: DeviceLatest = {
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

const DEFAULT_DAILY_REPORT: DailyReport = {
  date: '-',
  feed_count: 0,
  refill_count: 0,
  alert_count: 0,
  avg_temp_c: 0,
  avg_hum_rh: 0,
  avg_voc_index: 0,
  notes: [],
};

interface HistoryQueryParams {
  from?: number;
  to?: number;
  step?: number;
}

interface SelectOptionItem {
  value: string;
}

@Entry
@Component
struct Index {
  private readonly api: PetHomeApi = new PetHomeApi();

  @State latest: DeviceLatest = DEFAULT_LATEST;
  @State events: DeviceEvent[] = [];
  @State history: HistoryPoint[] = [];
  @State dailyReport: DailyReport = DEFAULT_DAILY_REPORT;
  @State isLoading: boolean = false;
  @State message: string = '等待数据刷新';

  @State feedTargetInput: string = '50';
  @State foodThresholdInput: string = '50';
  @State sampleIntervalInput: string = '1000';
  @State publishIntervalInput: string = '60000';
  @State changePublishMinIntervalInput: string = '5000';
  @State feedMaxRunInput: string = '20000';
  @State waterDebounceInput: string = '3000';
  @State waterOkHoldInput: string = '2000';
  @State pumpMaxRunInput: string = '30000';
  @State pumpCooldownInput: string = '60000';
  @State vocOnThresholdInput: string = '250';
  @State vocOffThresholdInput: string = '200';
  @State vocDebounceInput: string = '5000';
  @State vocOkHoldInput: string = '10000';
  @State fanMinOnInput: string = '60000';
  @State fanMaxOnInput: string = '1800000';
  @State lightOnThresholdInput: string = '100';
  @State lightOffThresholdInput: string = '150';
  @State nightModeEnabled: boolean = false;
  @State nightModeStartInput: string = '22:00';
  @State nightModeEndInput: string = '07:00';

  @State historyFromInput: string = '';
  @State historyToInput: string = '';
  @State historyStepInput: string = '60';

  @State tempHighThresholdInput: string = '30';
  @State tempLowThresholdInput: string = '10';
  @State humHighThresholdInput: string = '70';
  @State humLowThresholdInput: string = '30';

  @State deviceIdList: string[] = ['69982a337f2e6c302f55fae1_PetHome_01'];
  @State selectedDeviceId: string = '69982a337f2e6c302f55fae1_PetHome_01';
  @State newDeviceIdInput: string = '';

  @State apGatewayInput: string = 'http://192.168.4.1';
  @State apStatusText: string = '未读取';
  @State apSsidInput: string = '';
  @State apPwdInput: string = '';
  @State apScanList: string[] = [];

  private pref: dataPreferences.Preferences | null = null;
  private canvasSettings = new RenderingContextSettings(true);
  private canvasCtx = new CanvasRenderingContext2D(this.canvasSettings);
  @State currentMetric: string = 'temp_c';

  async aboutToAppear(): Promise<void> {
    const context = this.getUIContext().getHostContext() as Context;
    try {
      this.pref = await dataPreferences.getPreferences(context, 'PetHomePrefs');
      const savedListStr = await this.pref.get('deviceIdList', '["69982a337f2e6c302f55fae1_PetHome_01"]') as string;
      const parsed = JSON.parse(savedListStr) as string[];
      if (Array.isArray(parsed) && parsed.length > 0) {
        this.deviceIdList = parsed;
      }

      const cloudListResult = await this.api.getDeviceList();
      if (cloudListResult.ok && cloudListResult.data.length > 0) {
        this.deviceIdList = cloudListResult.data;
        await this.pref.put('deviceIdList', JSON.stringify(this.deviceIdList));
        await this.pref.flush();
      }

      const savedSelected = await this.pref.get('selectedDeviceId', this.deviceIdList[0]) as string;
      if (this.deviceIdList.includes(savedSelected)) {
        this.selectedDeviceId = savedSelected;
      } else {
        this.selectedDeviceId = this.deviceIdList[0];
      }
    } catch(e) {
       // 加载失败时使用默认值
    }
    this.api.setDevice(this.selectedDeviceId);
    this.refreshAll();
  }

  private getHistoryParams(): HistoryQueryParams {
    const from = Date.parse(this.historyFromInput);
    const to = Date.parse(this.historyToInput);
    const step = Number(this.historyStepInput);
    const params: HistoryQueryParams = {};
    if (Number.isFinite(from)) {
      params.from = from;
    }
    if (Number.isFinite(to)) {
      params.to = to;
    }
    if (Number.isFinite(step) && step > 0) {
      params.step = step;
    }
    return params;
  }

  private async refreshAll(): Promise<void> {
    this.isLoading = true;
    const latestResult = await this.api.getLatest();
    if (latestResult.ok) {
      const latestText = JSON.stringify(latestResult.data);
      this.latest = JSON.parse(latestText) as DeviceLatest;
    }

    const eventResult = await this.api.getEvents();
    if (eventResult.ok) {
      this.events = eventResult.data;
    }

    const p = this.getHistoryParams();
    const historyResult = await this.api.getHistory(this.currentMetric, p.from, p.to, p.step);
    if (historyResult.ok) {
      this.history = historyResult.data;
    }
    this.drawChart();

    const reportResult = await this.api.getDailyReport();
    if (reportResult.ok) {
      const reportText = JSON.stringify(reportResult.data);
      this.dailyReport = JSON.parse(reportText) as DailyReport;
    }

    this.isLoading = false;
    this.message = latestResult.ok
      ? `(${this.selectedDeviceId.substring(0, 8)}...) 刷新成功 ${this.formatTs(Date.now())}`
      : `刷新失败: ${latestResult.message}`;
  }

  private async onDeviceSwitch(deviceId: string): Promise<void> {
    if (this.selectedDeviceId !== deviceId) {
      this.selectedDeviceId = deviceId;
      if (this.pref) {
        await this.pref.put('selectedDeviceId', deviceId);
        await this.pref.flush();
      }
      this.api.setDevice(deviceId);
      this.refreshAll();
    }
  }

  private async onBindDevice(): Promise<void> {
    const trimmed = this.newDeviceIdInput.trim();
    if (trimmed.length === 0) {
      this.message = '请输入设备ID';
      return;
    }

    this.isLoading = true;
    const cloudListResult = await this.api.getDeviceList();
    this.isLoading = false;
    if (!cloudListResult.ok) {
      this.message = `读取云端设备列表失败: ${cloudListResult.message}`;
      return;
    }
    if (!cloudListResult.data.includes(trimmed)) {
      this.message = '云端未找到该设备ID，请先在 IoTDA 控制台确认设备已创建';
      return;
    }

    this.deviceIdList = cloudListResult.data;
    if (this.pref) {
      await this.pref.put('deviceIdList', JSON.stringify(this.deviceIdList));
      await this.pref.flush();
    }
    await this.onDeviceSwitch(trimmed);
    this.newDeviceIdInput = '';
    this.message = `绑定成功: ${trimmed}`;
  }

  private async toggleLed(): Promise<void> {
    await this.sendActuate('led_on', !this.latest.led_on);
  }

  private async toggleFan(): Promise<void> {
    await this.sendActuate('fan_on', !this.latest.fan_on);
  }

  private async togglePump(): Promise<void> {
    await this.sendActuate('pump_on', !this.latest.pump_on);
  }

  private async sendActuate(key: string, value: boolean): Promise<void> {
    this.isLoading = true;
    const payload: ActuateCommandPayload = {};
    if (key === 'led_on') {
      payload.led_on = value;
    } else if (key === 'fan_on') {
      payload.fan_on = value;
    } else if (key === 'pump_on') {
      payload.pump_on = value;
    }
    const result = await this.api.sendActuateCommand(payload);
    this.isLoading = false;
    this.message = result.ok
      ? `命令回执: ${this.formatCommandResult(result.data.result)} (${result.message})`
      : `命令失败: ${result.message}`;
    await this.refreshAll();
  }

  private async feedOnce(): Promise<void> {
    const feedTarget = clampConfigValue('feed_target_g', Number(this.feedTargetInput));
    this.feedTargetInput = `${feedTarget}`;
    this.isLoading = true;
    const result = await this.api.sendFeedCommand(feedTarget);
    this.isLoading = false;
    this.message = result.ok
      ? `投喂回执: ${this.formatCommandResult(result.data.result)} (${result.message})`
      : `投喂失败: ${result.message}`;
    await this.refreshAll();
  }

  private async saveConfig(): Promise<void> {
    const sampleInterval = clampConfigValue('sample_interval_ms', Number(this.sampleIntervalInput));
    const publishInterval = clampConfigValue('publish_interval_ms', Number(this.publishIntervalInput));
    const changePublishMinInterval =
      clampConfigValue('change_publish_min_interval_ms', Number(this.changePublishMinIntervalInput));
    const feedTarget = clampConfigValue('feed_target_g', Number(this.feedTargetInput));
    const foodLow = clampConfigValue('food_low_threshold_g', Number(this.foodThresholdInput));
    const feedMaxRun = clampConfigValue('feed_max_run_ms', Number(this.feedMaxRunInput));
    const waterDebounce = clampConfigValue('water_debounce_ms', Number(this.waterDebounceInput));
    const waterOkHold = clampConfigValue('water_ok_hold_ms', Number(this.waterOkHoldInput));
    const pumpMaxRun = clampConfigValue('pump_max_run_ms', Number(this.pumpMaxRunInput));
    const pumpCooldown = clampConfigValue('pump_cooldown_ms', Number(this.pumpCooldownInput));
    const vocOn = clampConfigValue('voc_on_threshold', Number(this.vocOnThresholdInput));
    const vocOff = clampConfigValue('voc_off_threshold', Number(this.vocOffThresholdInput));
    const vocDebounce = clampConfigValue('voc_debounce_ms', Number(this.vocDebounceInput));
    const vocOkHold = clampConfigValue('voc_ok_hold_ms', Number(this.vocOkHoldInput));
    const fanMinOn = clampConfigValue('fan_min_on_ms', Number(this.fanMinOnInput));
    const fanMaxOn = clampConfigValue('fan_max_on_ms', Number(this.fanMaxOnInput));
    const lightOn = clampConfigValue('light_on_threshold', Number(this.lightOnThresholdInput));
    const lightOff = clampConfigValue('light_off_threshold', Number(this.lightOffThresholdInput));
    const tempHigh = clampConfigValue('temp_high_threshold_c', Number(this.tempHighThresholdInput));
    const tempLow = clampConfigValue('temp_low_threshold_c', Number(this.tempLowThresholdInput));
    const humHigh = clampConfigValue('hum_high_threshold_rh', Number(this.humHighThresholdInput));
    const humLow = clampConfigValue('hum_low_threshold_rh', Number(this.humLowThresholdInput));

    this.sampleIntervalInput = `${sampleInterval}`;
    this.publishIntervalInput = `${publishInterval}`;
    this.changePublishMinIntervalInput = `${changePublishMinInterval}`;
    this.feedTargetInput = `${feedTarget}`;
    this.foodThresholdInput = `${foodLow}`;
    this.feedMaxRunInput = `${feedMaxRun}`;
    this.waterDebounceInput = `${waterDebounce}`;
    this.waterOkHoldInput = `${waterOkHold}`;
    this.pumpMaxRunInput = `${pumpMaxRun}`;
    this.pumpCooldownInput = `${pumpCooldown}`;
    this.vocOnThresholdInput = `${vocOn}`;
    this.vocOffThresholdInput = `${vocOff}`;
    this.vocDebounceInput = `${vocDebounce}`;
    this.vocOkHoldInput = `${vocOkHold}`;
    this.fanMinOnInput = `${fanMinOn}`;
    this.fanMaxOnInput = `${fanMaxOn}`;
    this.lightOnThresholdInput = `${lightOn}`;
    this.lightOffThresholdInput = `${lightOff}`;
    this.tempHighThresholdInput = `${tempHigh}`;
    this.tempLowThresholdInput = `${tempLow}`;
    this.humHighThresholdInput = `${humHigh}`;
    this.humLowThresholdInput = `${humLow}`;

    const configPayload: SetConfigPayload = {};
    configPayload.sample_interval_ms = sampleInterval;
    configPayload.publish_interval_ms = publishInterval;
    configPayload.change_publish_min_interval_ms = changePublishMinInterval;
    configPayload.food_low_threshold_g = foodLow;
    configPayload.feed_target_g = feedTarget;
    configPayload.feed_max_run_ms = feedMaxRun;
    configPayload.water_debounce_ms = waterDebounce;
    configPayload.water_ok_hold_ms = waterOkHold;
    configPayload.pump_max_run_ms = pumpMaxRun;
    configPayload.pump_cooldown_ms = pumpCooldown;
    configPayload.voc_on_threshold = vocOn;
    configPayload.voc_off_threshold = vocOff;
    configPayload.voc_debounce_ms = vocDebounce;
    configPayload.voc_ok_hold_ms = vocOkHold;
    configPayload.fan_min_on_ms = fanMinOn;
    configPayload.fan_max_on_ms = fanMaxOn;
    configPayload.light_on_threshold = lightOn;
    configPayload.light_off_threshold = lightOff;
    configPayload.temp_high_threshold_c = tempHigh;
    configPayload.temp_low_threshold_c = tempLow;
    configPayload.hum_high_threshold_rh = humHigh;
    configPayload.hum_low_threshold_rh = humLow;
    configPayload.night_mode_enabled = this.nightModeEnabled;

    const result = await this.api.sendSetConfig(configPayload);
    this.message = result.ok
      ? `配置下发结果: ${this.formatCommandResult(result.data.result)} (${result.message})`
      : `配置下发失败: ${result.message}`;
    await this.refreshAll();
  }

  private async saveNightWindow(): Promise<void> {
    const start = normalizeTimeValue(this.nightModeStartInput, '22:00');
    const end = normalizeTimeValue(this.nightModeEndInput, '07:00');
    this.nightModeStartInput = start;
    this.nightModeEndInput = end;

    if (!isValidTimeValue(start) || !isValidTimeValue(end)) {
      this.message = '时间格式必须是“时:分”，例如 23:30';
      return;
    }

    this.isLoading = true;
    const modePayload: SetConfigPayload = {};
    modePayload.night_mode_enabled = this.nightModeEnabled;
    const modeResult = await this.api.sendSetConfig(modePayload);
    if (!modeResult.ok) {
      this.isLoading = false;
      this.message = `夜间模式开关下发失败: ${modeResult.message}`;
      await this.refreshAll();
      return;
    }

    const windowResult = await this.api.sendSetNightWindow(start, end);
    this.isLoading = false;
    this.message = windowResult.ok
      ? `夜间时间窗下发结果: ${start}-${end} (${windowResult.message})`
      : `夜间时间窗下发失败: ${windowResult.message}`;
    await this.refreshAll();
  }

  private async loadApStatus(): Promise<void> {
    const result = await this.api.getApStatus(this.apGatewayInput);
    if (!result.ok) {
      this.apStatusText = `读取失败: ${result.message}`;
      return;
    }

    const ssid = String(result.data['ap_ssid'] ?? '-');
    const pwd = String(result.data['ap_pwd'] ?? '-');
    const provisioned = Boolean(result.data['provisioned'] ?? false);
    this.apStatusText = `配网热点: ${ssid} / 密码: ${pwd} / 已配置: ${provisioned ? '是' : '否'}`;
  }

  private async scanApNetworks(): Promise<void> {
    const result = await this.api.scanProvisionNetworks(this.apGatewayInput);
    if (!result.ok) {
      this.message = `扫描失败: ${result.message}`;
      return;
    }
    const nextList: string[] = [];
    for (let i = 0; i < result.data.length; i++) {
      const item = result.data[i];
      const name = String(item['ssid'] ?? '');
      if (name.length > 0) {
        nextList.push(name);
      }
    }
    this.apScanList = nextList;
    this.message = `扫描到 ${nextList.length} 个无线网络`;
  }

  private async submitApConfig(): Promise<void> {
    if (this.apSsidInput.trim().length === 0) {
      this.message = '请先输入家庭无线网络名称';
      return;
    }

    const result = await this.api.submitProvisionConfig(
      this.apGatewayInput,
      this.apSsidInput,
      this.apPwdInput,
    );
    this.message = result.ok ? '设备已接收配网参数，等待重启联网' : `配网失败: ${result.message}`;
    await this.loadApStatus();
  }

  private statusSummary(): string {
    const base = this.latest.online ? '在线' : '离线';
    const err = this.latest.last_error.length > 0 ? ` | 错误:${this.latest.last_error}` : '';
    return `${base} | 最后更新:${this.formatTs(this.latest.ts)} | 固件:${this.latest.fw_version}${err}`;
  }

  private formatCommandResult(result: string): string {
    if (result === 'OK') {
      return '成功';
    }
    if (result === 'ACCEPTED') {
      return '已受理';
    }
    if (result === 'REJECTED') {
      return '已拒绝';
    }
    if (result === 'FAILED') {
      return '失败';
    }
    return result;
  }

  private formatWaterState(state: string): string {
    if (state === 'LOW') {
      return '偏低';
    }
    if (state === 'OK') {
      return '正常';
    }
    return state;
  }

  private formatEventLevel(level: string): string {
    if (level === 'ERROR') {
      return '严重';
    }
    if (level === 'WARN') {
      return '警告';
    }
    if (level === 'INFO') {
      return '提示';
    }
    return level;
  }

  private formatEventType(eventType: string): string {
    if (eventType === 'LOW_WATER') {
      return '低水位';
    }
    if (eventType === 'LOW_FOOD') {
      return '低余粮';
    }
    if (eventType === 'CLOUD_OFFLINE') {
      return '设备离线';
    }
    if (eventType === 'SENSOR_FAULT') {
      return '传感异常';
    }
    if (eventType === 'FAN_OVERTIME') {
      return '风扇超时';
    }
    if (eventType === 'FEED_FAILED') {
      return '投喂失败';
    }
    if (eventType === 'WATER_REFILL_FAILED') {
      return '补水失败';
    }
    if (eventType === 'WIFI_OFFLINE') {
      return 'WiFi断连';
    }
    if (eventType === 'CONFIG_CONFIRMED') {
      return '配置已保存';
    }
    if (eventType === 'CONFIG_RESET_DEFAULT') {
      return '配置已重置';
    }
    return eventType;
  }

  private twoDigits(value: number): string {
    return value < 10 ? `0${value}` : `${value}`;
  }

  private formatTs(ts: number): string {
    if (ts <= 0) {
      return '-';
    }
    const date = new Date(ts);
    return `${this.twoDigits(date.getHours())}:${this.twoDigits(date.getMinutes())}:${this.twoDigits(date.getSeconds())}`;
  }

  private buildDeviceOptions(): SelectOptionItem[] {
    const options: SelectOptionItem[] = [];
    for (let i = 0; i < this.deviceIdList.length; i++) {
      options.push({ value: this.deviceIdList[i] });
    }
    return options;
  }

  @Builder
  private renderHeaderAndActions(): void {
    Text('智能宠物屋控制台')
      .fontSize(26)
      .fontWeight(FontWeight.Bold)

    Row({ space: 8 }) {
      Select(this.buildDeviceOptions())
        .selected(this.deviceIdList.indexOf(this.selectedDeviceId))
        .value(this.selectedDeviceId)
        .onSelect((index: number) => {
          this.onDeviceSwitch(this.deviceIdList[index]);
        })
    }

    Row({ space: 8 }) {
      TextInput({ placeholder: '新增/切换 DeviceID', text: this.newDeviceIdInput })
        .width('60%')
        .onChange((v) => this.newDeviceIdInput = v)
      Button('绑定')
        .onClick(() => this.onBindDevice())
    }

    Text(this.statusSummary())
      .fontSize(14)
      .fontColor(this.latest.online ? '#0f7b0f' : '#c52222')

    Text(this.message)
      .fontSize(12)
      .fontColor('#666666')

    Row({ space: 10 }) {
      Button('刷新')
        .onClick(() => this.refreshAll())
      Button(this.latest.led_on ? '关灯' : '开灯')
        .onClick(() => this.toggleLed())
      Button(this.latest.fan_on ? '关风扇' : '开风扇')
        .onClick(() => this.toggleFan())
    }

    Row({ space: 10 }) {
      Button(this.latest.pump_on ? '停止补水' : '开始补水')
        .onClick(() => this.togglePump())
      Button('投喂一次')
        .onClick(() => this.feedOnce())
    }
  }

  @Builder
  private renderRealtimeCard(): void {
    Column({ space: 6 }) {
      Text('实时指标').fontSize(18).fontWeight(FontWeight.Medium)
      Text(`温度: ${this.latest.temp_c.toFixed(1)} 摄氏度`)
      Text(`湿度: ${this.latest.hum_rh.toFixed(1)} %`)
      Text(`光照: ${this.latest.lux.toFixed(0)} 勒克斯`)
      Text(`余粮: ${this.latest.food_g.toFixed(1)} 克`)
      Text(`水位: ${this.formatWaterState(this.latest.water_level_state)}`)
      Text(`气味指数: ${this.latest.voc_index}（总挥发性有机物 ${this.latest.tvoc_ppb} 十亿分之一）`)
    }
    .padding(12)
    .borderRadius(12)
    .backgroundColor('#f4f6f8')
  }

  @Builder
  private renderConfigCard(): void {
    Column({ space: 6 }) {
      Text('参数配置').fontSize(18).fontWeight(FontWeight.Medium)

      Row({ space: 8 }) {
        Text('采样周期(毫秒):').width('35%')
        TextInput({ text: this.sampleIntervalInput })
          .type(InputType.Number)
          .onChange((v: string) => this.sampleIntervalInput = v)
      }
      Row({ space: 8 }) {
        Text('上报周期(毫秒):').width('35%')
        TextInput({ text: this.publishIntervalInput })
          .type(InputType.Number)
          .onChange((v: string) => this.publishIntervalInput = v)
      }
      Row({ space: 8 }) {
        Text('变化上报最小间隔(毫秒):').width('35%')
        TextInput({ text: this.changePublishMinIntervalInput })
          .type(InputType.Number)
          .onChange((v: string) => this.changePublishMinIntervalInput = v)
      }
      Row({ space: 8 }) {
        Text('单次投喂(克):').width('35%')
        TextInput({ text: this.feedTargetInput })
          .type(InputType.Number)
          .onChange((v: string) => this.feedTargetInput = v)
      }
      Row({ space: 8 }) {
        Text('低粮阈值(克):').width('35%')
        TextInput({ text: this.foodThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.foodThresholdInput = v)
      }
      Row({ space: 8 }) {
        Text('投喂超时(毫秒):').width('35%')
        TextInput({ text: this.feedMaxRunInput })
          .type(InputType.Number)
          .onChange((v: string) => this.feedMaxRunInput = v)
      }
      Row({ space: 8 }) {
        Text('补水超时(毫秒):').width('35%')
        TextInput({ text: this.pumpMaxRunInput })
          .type(InputType.Number)
          .onChange((v: string) => this.pumpMaxRunInput = v)
      }
      Row({ space: 8 }) {
        Text('水位去抖(毫秒):').width('35%')
        TextInput({ text: this.waterDebounceInput })
          .type(InputType.Number)
          .onChange((v: string) => this.waterDebounceInput = v)
      }
      Row({ space: 8 }) {
        Text('水位恢复保持(毫秒):').width('35%')
        TextInput({ text: this.waterOkHoldInput })
          .type(InputType.Number)
          .onChange((v: string) => this.waterOkHoldInput = v)
      }
      Row({ space: 8 }) {
        Text('水泵冷却(毫秒):').width('35%')
        TextInput({ text: this.pumpCooldownInput })
          .type(InputType.Number)
          .onChange((v: string) => this.pumpCooldownInput = v)
      }
      Row({ space: 8 }) {
        Text('气味开阈值:').width('35%')
        TextInput({ text: this.vocOnThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.vocOnThresholdInput = v)
      }
      Row({ space: 8 }) {
        Text('气味关阈值:').width('35%')
        TextInput({ text: this.vocOffThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.vocOffThresholdInput = v)
      }
      Row({ space: 8 }) {
        Text('气味去抖(毫秒):').width('35%')
        TextInput({ text: this.vocDebounceInput })
          .type(InputType.Number)
          .onChange((v: string) => this.vocDebounceInput = v)
      }
      Row({ space: 8 }) {
        Text('气味恢复保持(毫秒):').width('35%')
        TextInput({ text: this.vocOkHoldInput })
          .type(InputType.Number)
          .onChange((v: string) => this.vocOkHoldInput = v)
      }
      Row({ space: 8 }) {
        Text('风扇最小开(毫秒):').width('35%')
        TextInput({ text: this.fanMinOnInput })
          .type(InputType.Number)
          .onChange((v: string) => this.fanMinOnInput = v)
      }
      Row({ space: 8 }) {
        Text('风扇最大开(毫秒):').width('35%')
        TextInput({ text: this.fanMaxOnInput })
          .type(InputType.Number)
          .onChange((v: string) => this.fanMaxOnInput = v)
      }
      Row({ space: 8 }) {
        Text('补光开启(勒克斯):').width('35%')
        TextInput({ text: this.lightOnThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.lightOnThresholdInput = v)
      }
      Row({ space: 8 }) {
        Text('补光关闭(勒克斯):').width('35%')
        TextInput({ text: this.lightOffThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.lightOffThresholdInput = v)
      }
      Row({ space: 8 }) {
        Text('温度高阈值(°C):').width('35%')
        TextInput({ text: this.tempHighThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.tempHighThresholdInput = v)
      }
      Row({ space: 8 }) {
        Text('温度低阈值(°C):').width('35%')
        TextInput({ text: this.tempLowThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.tempLowThresholdInput = v)
      }
      Row({ space: 8 }) {
        Text('湿度高阈值(%):').width('35%')
        TextInput({ text: this.humHighThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.humHighThresholdInput = v)
      }
      Row({ space: 8 }) {
        Text('湿度低阈值(%):').width('35%')
        TextInput({ text: this.humLowThresholdInput })
          .type(InputType.Number)
          .onChange((v: string) => this.humLowThresholdInput = v)
      }

      Button('下发完整配置').onClick(() => this.saveConfig())

      Row({ space: 8 }) {
        Text('夜间模式:').width('35%')
        Button(this.nightModeEnabled ? '已启用' : '已关闭')
          .onClick(() => this.nightModeEnabled = !this.nightModeEnabled)
      }
      Row({ space: 8 }) {
        Text('夜间开始(时:分):').width('35%')
        TextInput({ text: this.nightModeStartInput })
          .onChange((v: string) => this.nightModeStartInput = v)
      }
      Row({ space: 8 }) {
        Text('夜间结束(时:分):').width('35%')
        TextInput({ text: this.nightModeEndInput })
          .onChange((v: string) => this.nightModeEndInput = v)
      }
      Button('下发夜间时间窗').onClick(() => this.saveNightWindow())
    }
    .padding(12)
    .borderRadius(12)
    .backgroundColor('#f4f6f8')
  }

  @Builder
  private renderApCard(): void {
    Column({ space: 6 }) {
      Text('热点配网').fontSize(18).fontWeight(FontWeight.Medium)
      Row({ space: 8 }) {
        Text('网关:').width('20%')
        TextInput({ text: this.apGatewayInput })
          .onChange((v: string) => this.apGatewayInput = v)
      }
      Row({ space: 8 }) {
        Button('读取状态').onClick(() => this.loadApStatus())
        Button('扫描无线网络').onClick(() => this.scanApNetworks())
      }
      Text(this.apStatusText).fontSize(12).fontColor('#555555')

      Row({ space: 8 }) {
        Text('网络名称:').width('20%')
        TextInput({ text: this.apSsidInput })
          .onChange((v: string) => this.apSsidInput = v)
      }
      Row({ space: 8 }) {
        Text('密码:').width('20%')
        TextInput({ text: this.apPwdInput })
          .onChange((v: string) => this.apPwdInput = v)
      }
      Button('提交配网').onClick(() => this.submitApConfig())

      if (this.apScanList.length > 0) {
        Text('附近无线网络:').fontSize(12).fontColor('#555555')
        ForEach(this.apScanList, (ssid: string) => {
          Text(ssid)
            .fontSize(12)
            .onClick(() => this.apSsidInput = ssid)
        }, (ssid: string) => ssid)
      }
    }
    .padding(12)
    .borderRadius(12)
    .backgroundColor('#f4f6f8')
  }

  @Builder
  private renderEventsCard(): void {
    Column({ space: 6 }) {
      Text('告警事件').fontSize(18).fontWeight(FontWeight.Medium)
      if (this.events.length === 0) {
        Text('暂无事件')
      } else {
        ForEach(this.events, (item: DeviceEvent) => {
          Text(`[${this.formatTs(item.ts)}] ${this.formatEventLevel(item.level)} ${this.formatEventType(item.event_type)}`)
            .fontSize(13)
        }, (item: DeviceEvent) => `${item.ts}_${item.event_type}`)
      }
    }
    .padding(12)
    .borderRadius(12)
    .backgroundColor('#f4f6f8')
  }

  private async switchMetric(metric: string): Promise<void> {
    this.currentMetric = metric;
    this.isLoading = true;
    const p = this.getHistoryParams();
    const historyResult = await this.api.getHistory(metric, p.from, p.to, p.step);
    if (historyResult.ok) {
      this.history = historyResult.data;
    }
    this.isLoading = false;
    this.drawChart();
  }

  private drawChart(): void {
    if (!this.canvasCtx) return;
    const w = 300; 
    const h = 150; 
    const ctx = this.canvasCtx;
    ctx.clearRect(0, 0, w, h);
    if (this.history.length === 0) {
      ctx.font = '14px sans-serif';
      ctx.fillStyle = '#999';
      ctx.fillText('暂无曲线数据', 100, 75);
      return;
    }
    
    let minV = Number.MAX_VALUE;
    let maxV = Number.MIN_VALUE;
    for (const pt of this.history) {
      if (pt.value < minV) minV = pt.value;
      if (pt.value > maxV) maxV = pt.value;
    }
    if (minV === maxV) {
      minV -= 10;
      maxV += 10;
    }
    const range = maxV - minV;
    const padding = 15;
    
    ctx.strokeStyle = '#2f54eb';
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (let i = 0; i < this.history.length; i++) {
      const pt = this.history[i];
      const x = padding + (w - 2 * padding) * (i / (this.history.length - 1 || 1));
      const y = h - padding - (pt.value - minV) / range * (h - 2 * padding);
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
    
    ctx.fillStyle = '#666';
    ctx.font = '10px sans-serif';
    ctx.fillText(maxV.toFixed(1), 0, 15);
    ctx.fillText(minV.toFixed(1), 0, h - 2);
  }

  @Builder
  private renderHistoryCard(): void {
    Column({ space: 8 }) {
      Text('历史曲线数据').fontSize(18).fontWeight(FontWeight.Medium)
      Row({ space: 8 }) {
        TextInput({ placeholder: '开始如: 2026-02-20T00:00:00', text: this.historyFromInput })
          .width('48%')
          .onChange((v: string) => this.historyFromInput = v)
        TextInput({ placeholder: '结束如: 2026-02-20T23:59:59', text: this.historyToInput })
          .width('48%')
          .onChange((v: string) => this.historyToInput = v)
      }
      Row({ space: 8 }) {
        Text('粒度(秒):').width('20%')
        TextInput({ text: this.historyStepInput })
          .width('30%')
          .type(InputType.Number)
          .onChange((v: string) => this.historyStepInput = v)
        Button('查询').onClick(() => { this.switchMetric(this.currentMetric); })
      }
      Row({ space: 10 }) {
        Button('温度').onClick(() => this.switchMetric('temp_c')).backgroundColor(this.currentMetric === 'temp_c' ? '#2f54eb' : '#ccc')
        Button('湿度').onClick(() => this.switchMetric('hum_rh')).backgroundColor(this.currentMetric === 'hum_rh' ? '#2f54eb' : '#ccc')
        Button('气味').onClick(() => this.switchMetric('voc_index')).backgroundColor(this.currentMetric === 'voc_index' ? '#2f54eb' : '#ccc')
      }
      Canvas(this.canvasCtx)
        .width('100%')
        .height(150)
        .onReady(() => {
          this.drawChart();
        })
    }
    .padding(12)
    .borderRadius(12)
    .backgroundColor('#f4f6f8')
  }

  @Builder
  private renderDailyReportCard(): void {
    Column({ space: 6 }) {
      Text('当日日报').fontSize(18).fontWeight(FontWeight.Medium)
      Text(`日期: ${this.dailyReport.date}`)
      Text(`投喂次数: ${this.dailyReport.feed_count}`)
      Text(`补水次数: ${this.dailyReport.refill_count}`)
      Text(`告警次数: ${this.dailyReport.alert_count}`)
      Text(`平均温度: ${this.dailyReport.avg_temp_c.toFixed(1)} 摄氏度`)
      Text(`平均湿度: ${this.dailyReport.avg_hum_rh.toFixed(1)} %`)
      Text(`平均气味指数: ${this.dailyReport.avg_voc_index.toFixed(0)}`)
    }
    .padding(12)
    .borderRadius(12)
    .backgroundColor('#f4f6f8')
  }

  @Builder
  private renderLoadingState(): void {
    if (this.isLoading) {
      Text('处理中...')
        .fontColor('#2f54eb')
    }
  }

  build() {
    Scroll() {
      Column({ space: 12 }) {
        this.renderHeaderAndActions()
        this.renderRealtimeCard()
        this.renderConfigCard()
        this.renderApCard()
        this.renderEventsCard()
        this.renderHistoryCard()
        this.renderDailyReportCard()
        this.renderLoadingState()
      }
      .padding(16)
      .width('100%')
    }
    .scrollBar(BarState.Auto)
  }
}



