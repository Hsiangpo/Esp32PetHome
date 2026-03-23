import { CommandResponse } from '../model/PetHomeModelsTs';

type JsonPrimitive = string | number | boolean | null;
type JsonValue = JsonPrimitive | JsonObject | JsonObject[] | string[] | number[] | boolean[];
interface JsonObject { [key: string]: JsonValue; }

export interface HttpResultLike<T> {
  ok: boolean;
  data: T;
  message: string;
}

interface IotdaCommandResponseParas {
  reason?: string;
}

interface IotdaCommandResponseBody {
  result_code?: number | string;
  response_name?: string;
  paras?: IotdaCommandResponseParas;
}

export interface IotdaCommandResponse {
  command_id?: string;
  error_code?: string;
  error_msg?: string;
  response?: IotdaCommandResponseBody;
  status?: string;
  command_status?: string;
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

function toStringValue(value: JsonValue | undefined, fallback: string): string {
  if (typeof value === 'string') {
    return value;
  }
  return fallback;
}

function buildFailedResult(reason: string, requestId?: string): CommandResponse {
  return {
    request_id: requestId ?? `req_${Date.now()}`,
    result: 'FAILED',
    reason,
    ts: Date.now(),
  };
}

function buildAcceptedResult(requestId?: string): CommandResponse {
  return {
    request_id: requestId ?? `req_${Date.now()}`,
    result: 'ACCEPTED',
    reason: 'COMMAND_ACCEPTED',
    ts: Date.now(),
  };
}

export function mapIotdaCommandResponse(
  result: HttpResultLike<IotdaCommandResponse>,
  cmd: string,
): HttpResultLike<CommandResponse> {
  if (!result.ok) {
    return {
      ok: false,
      data: buildFailedResult(result.message),
      message: result.message,
    };
  }

  const raw = result.data;
  if (raw.error_code !== undefined && raw.error_code.length > 0) {
    return {
      ok: false,
      data: buildFailedResult(
        `${raw.error_code}:${raw.error_msg ?? 'IOTDA_ERROR'}`,
        raw.command_id,
      ),
      message: raw.error_msg ?? raw.error_code,
    };
  }

  if (!raw.response || raw.response.result_code === undefined || raw.response.result_code === null) {
    return {
      ok: true,
      data: buildAcceptedResult(raw.command_id),
      message: '云端已受理，等待设备回执',
    };
  }

  const resultCode = toNumber(raw.response.result_code, Number.NaN);
  if (!Number.isFinite(resultCode)) {
    return {
      ok: false,
      data: buildFailedResult('INVALID_RESULT_CODE', raw.command_id),
      message: '设备回执格式异常',
    };
  }

  let mappedResult: 'OK' | 'REJECTED' | 'FAILED' = 'FAILED';
  if (resultCode === 0) {
    mappedResult = 'OK';
  } else if (resultCode === 1) {
    mappedResult = 'REJECTED';
  }

  const reason = raw.response.paras && raw.response.paras.reason
    ? raw.response.paras.reason
    : (raw.response.response_name ?? `${cmd}_accepted`);
  const commandResult: CommandResponse = {
    request_id: raw.command_id ?? `req_${Date.now()}`,
    result: mappedResult,
    reason,
    ts: Date.now(),
  };
  return {
    ok: mappedResult === 'OK' || mappedResult === 'REJECTED',
    data: commandResult,
    message: mappedResult === 'OK'
      ? '执行成功'
      : (mappedResult === 'REJECTED' ? '设备已拒绝' : commandResult.reason),
  };
}

export function mapIotdaCommandQueryResponse(
  queryResult: HttpResultLike<IotdaCommandResponse>,
  cmd: string,
  commandId: string,
): HttpResultLike<CommandResponse> {
  const mapped = mapIotdaCommandResponse(queryResult, cmd);
  if (!mapped.ok || mapped.data.result !== 'ACCEPTED') {
    return mapped;
  }

  const raw = queryResult.data;
  const queryStatus = toStringValue(raw.status ?? raw.command_status, '').toUpperCase();
  if (queryStatus.length === 0) {
    return mapped;
  }

  if (queryStatus === 'SUCCESS' || queryStatus === 'SUCCESSFUL') {
    return {
      ok: true,
      data: {
        request_id: commandId,
        result: 'OK',
        reason: queryStatus,
        ts: Date.now(),
      },
      message: '执行成功',
    };
  }

  if (
    queryStatus === 'FAILED' ||
    queryStatus === 'TIMEOUT' ||
    queryStatus === 'CANCELED' ||
    queryStatus === 'EXPIRED' ||
    queryStatus === 'REJECTED'
  ) {
    return {
      ok: queryStatus === 'REJECTED',
      data: {
        request_id: commandId,
        result: queryStatus === 'REJECTED' ? 'REJECTED' : 'FAILED',
        reason: queryStatus,
        ts: Date.now(),
      },
      message: queryStatus === 'REJECTED' ? '设备已拒绝' : '设备执行失败',
    };
  }

  return {
    ok: true,
    data: buildAcceptedResult(commandId),
    message: `设备执行中(${queryStatus})`,
  };
}
