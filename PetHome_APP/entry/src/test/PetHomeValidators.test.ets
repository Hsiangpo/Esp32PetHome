import { describe, it, expect } from '@ohos/hypium';
import {
  buildFeedCommandPayload,
  buildProvisionConfigPayload,
  clampConfigValue,
  configRangeMap,
  isValidTimeValue,
  normalizeTimeValue,
  normalizeApGateway,
} from '../main/ets/utils/PetHomeValidators';

export default function petHomeValidatorsTest() {
  describe('petHomeValidatorsTest', () => {
    it('clamp_food_threshold_in_range', 0, () => {
      const min = configRangeMap.food_low_threshold_g.min;
      const max = configRangeMap.food_low_threshold_g.max;
      expect(clampConfigValue('food_low_threshold_g', -10)).assertEqual(min);
      expect(clampConfigValue('food_low_threshold_g', 9999)).assertEqual(max);
    });

    it('clamp_feed_target_in_range', 0, () => {
      const value = clampConfigValue('feed_target_g', 83);
      expect(value).assertEqual(83);
    });

    it('clamp_temp_hum_in_range', 0, () => {
      const tempHighMax = configRangeMap.temp_high_threshold_c.max;
      const humLowMin = configRangeMap.hum_low_threshold_rh.min;
      expect(clampConfigValue('temp_high_threshold_c', 60)).assertEqual(tempHighMax);
      expect(clampConfigValue('hum_low_threshold_rh', -10)).assertEqual(humLowMin);
    });

    it('build_feed_command_payload', 0, () => {
      const payload = buildFeedCommandPayload('req_001', 120);
      expect(payload.cmd).assertEqual('feed_once');
      expect(payload.params.feed_target_g).assertEqual(120);
      expect(payload.request_id).assertEqual('req_001');
    });

    it('normalize_ap_gateway_should_trim_trailing_slash', 0, () => {
      const normalized = normalizeApGateway('http://192.168.4.1///');
      expect(normalized).assertEqual('http://192.168.4.1');
    });

    it('build_provision_payload_should_trim_ssid', 0, () => {
      const payload = buildProvisionConfigPayload('  HomeWiFi  ', '12345678');
      expect(payload.ssid).assertEqual('HomeWiFi');
      expect(payload.pwd).assertEqual('12345678');
    });

    it('time_value_validation', 0, () => {
      expect(isValidTimeValue('22:00')).assertTrue();
      expect(isValidTimeValue('07:59')).assertTrue();
      expect(isValidTimeValue('24:00')).assertFalse();
      expect(isValidTimeValue('ab:cd')).assertFalse();
    });

    it('normalize_time_value_should_fallback', 0, () => {
      expect(normalizeTimeValue(' 23:30 ', '22:00')).assertEqual('23:30');
      expect(normalizeTimeValue('wrong', '22:00')).assertEqual('22:00');
    });
  });
}
