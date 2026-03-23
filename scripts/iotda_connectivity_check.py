#!/usr/bin/env python3
"""IoTDA 连通性校验脚本（App 侧 AK/SK + V11-HMAC-SHA256）。"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import hmac
import json
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

ALGORITHM = "V11-HMAC-SHA256"
SERVICE_NAME = "iotda"


def load_defaults_from_ts(ts_path: Path) -> dict[str, str]:
  text = ts_path.read_text(encoding="utf-8")
  pairs = dict(re.findall(r"(baseUrl|region|instanceId|projectId|deviceId|ak|sk):\s*'([^']+)'", text))
  required = ["baseUrl", "region", "instanceId", "projectId", "deviceId", "ak", "sk"]
  missing = [key for key in required if key not in pairs]
  if missing:
    raise RuntimeError(f"缺少必要默认参数: {', '.join(missing)}")
  return pairs


def build_x_sdk_date() -> str:
  return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def rfc3986_encode(value: str) -> str:
  return urllib.parse.quote(value, safe="~-._")


def canonicalize_uri(path: str) -> str:
  decoded = urllib.parse.unquote(path)
  segments = decoded.split("/")
  encoded = "/".join(rfc3986_encode(seg) for seg in segments)
  if encoded.endswith("/"):
    return encoded
  return encoded + "/"


def canonicalize_query(raw_query: str) -> str:
  if not raw_query:
    return ""
  pairs = urllib.parse.parse_qsl(raw_query, keep_blank_values=True)
  pairs.sort(key=lambda item: (item[0], item[1]))
  return "&".join(f"{rfc3986_encode(k)}={rfc3986_encode(v)}" for k, v in pairs)


def sha256_hex(text: str) -> str:
  return hashlib.sha256(text.encode("utf-8")).hexdigest()


def build_authorization(
  *,
  method: str,
  canonical_uri: str,
  canonical_query: str,
  host: str,
  x_sdk_date: str,
  instance_id: str,
  region: str,
  ak: str,
  sk: str,
  body: str,
  content_type: str | None,
) -> str:
  headers_to_sign: list[tuple[str, str]] = [
    ("host", host),
    ("x-sdk-date", x_sdk_date),
    ("instance-id", instance_id),
  ]
  if content_type:
    headers_to_sign.append(("content-type", content_type))
  headers_to_sign.sort(key=lambda item: item[0])

  canonical_headers = "".join(f"{k}:{v.strip()}\n" for k, v in headers_to_sign)
  signed_headers = ";".join(k for k, _ in headers_to_sign)
  payload_hash = sha256_hex(body)

  canonical_request = "\n".join([
    method.upper(),
    canonicalize_uri(canonical_uri),
    canonical_query,
    canonical_headers,
    signed_headers,
    payload_hash,
  ])

  date_text = x_sdk_date[:8]
  info = f"{date_text}/{region}/{SERVICE_NAME}"
  string_to_sign = "\n".join([
    ALGORITHM,
    x_sdk_date,
    info,
    sha256_hex(canonical_request),
  ])

  # 与 ArkTS 侧实现保持一致：先派生十六进制，再作为字符串 HMAC key。
  prk = hmac.new(ak.encode("utf-8"), sk.encode("utf-8"), hashlib.sha256).digest()
  der = hmac.new(prk, (info + "\x01").encode("utf-8"), hashlib.sha256).digest()
  derived_hex = der.hex()
  signature = hmac.new(derived_hex.encode("utf-8"), string_to_sign.encode("utf-8"), hashlib.sha256).hexdigest()
  return f"{ALGORITHM} Credential={ak}/{info}, SignedHeaders={signed_headers}, Signature={signature}"


def request_iotda(
  *,
  base_url: str,
  region: str,
  instance_id: str,
  ak: str,
  sk: str,
  method: str,
  path_with_query: str,
  payload: dict[str, Any] | None,
) -> tuple[int, dict[str, Any] | str]:
  body = "" if payload is None else json.dumps(payload, ensure_ascii=False)
  content_type = "application/json" if payload is not None else None

  parsed_base = urllib.parse.urlparse(base_url)
  host = parsed_base.netloc

  qidx = path_with_query.find("?")
  canonical_uri = path_with_query if qidx < 0 else path_with_query[:qidx]
  raw_query = "" if qidx < 0 else path_with_query[qidx + 1 :]

  x_sdk_date = build_x_sdk_date()
  auth = build_authorization(
    method=method,
    canonical_uri=canonical_uri,
    canonical_query=canonicalize_query(raw_query),
    host=host,
    x_sdk_date=x_sdk_date,
    instance_id=instance_id,
    region=region,
    ak=ak,
    sk=sk,
    body=body,
    content_type=content_type,
  )

  headers = {
    "X-Sdk-Date": x_sdk_date,
    "Instance-Id": instance_id,
    "Authorization": auth,
    "Accept": "application/json",
  }
  data = None
  if payload is not None:
    headers["Content-Type"] = "application/json"
    data = body.encode("utf-8")

  url = base_url.rstrip("/") + path_with_query
  req = urllib.request.Request(url=url, data=data, headers=headers, method=method)
  try:
    with urllib.request.urlopen(req, timeout=15) as resp:
      status = int(resp.status)
      raw = resp.read().decode("utf-8", errors="replace")
  except urllib.error.HTTPError as err:
    status = int(err.code)
    raw = err.read().decode("utf-8", errors="replace")
  except Exception as err:  # noqa: BLE001
    raise RuntimeError(f"请求异常: {err}") from err

  try:
    return status, json.loads(raw)
  except json.JSONDecodeError:
    return status, raw


def to_json_text(value: Any) -> str:
  return json.dumps(value, ensure_ascii=False, indent=2)


def main() -> int:
  parser = argparse.ArgumentParser(description="IoTDA 连通性与状态校验")
  parser.add_argument("--defaults", default="PetHome_APP/entry/src/main/ets/config/iotdaDefaultsTs.ts", help="默认参数文件")
  parser.add_argument("--device-id", default="", help="可选覆盖设备ID")
  parser.add_argument("--wait-online", type=int, default=0, help="等待设备上线秒数，0 表示不等待")
  parser.add_argument("--poll-interval", type=int, default=5, help="等待上线轮询间隔秒数")
  parser.add_argument("--check-shadow", action="store_true", help="额外读取一次设备影子")
  parser.add_argument("--send-feed", action="store_true", help="发送一次投喂命令（默认不发送）")
  parser.add_argument("--feed-target", type=int, default=50, help="投喂目标克数")
  args = parser.parse_args()

  defaults = load_defaults_from_ts(Path(args.defaults))
  device_id = args.device_id.strip() or defaults["deviceId"]

  base_url = defaults["baseUrl"]
  region = defaults["region"]
  instance_id = defaults["instanceId"]
  project_id = defaults["projectId"]
  ak = defaults["ak"]
  sk = defaults["sk"]

  info_path = f"/v5/iot/{project_id}/devices/{device_id}"
  status, payload = request_iotda(
    base_url=base_url,
    region=region,
    instance_id=instance_id,
    ak=ak,
    sk=sk,
    method="GET",
    path_with_query=info_path,
    payload=None,
  )
  print(f"[INFO] GET device info status={status}")
  print(to_json_text(payload))

  if status < 200 or status >= 300:
    return 2

  online = str(payload.get("status", "")).upper() == "ONLINE" if isinstance(payload, dict) else False
  if args.wait_online > 0 and not online:
    deadline = time.time() + args.wait_online
    print(f"[WAIT] 当前离线，等待上线，超时 {args.wait_online}s")
    while time.time() < deadline:
      time.sleep(max(args.poll_interval, 1))
      st, pl = request_iotda(
        base_url=base_url,
        region=region,
        instance_id=instance_id,
        ak=ak,
        sk=sk,
        method="GET",
        path_with_query=info_path,
        payload=None,
      )
      state = str(pl.get("status", "")).upper() if isinstance(pl, dict) else "UNKNOWN"
      print(f"[WAIT] status={st}, device_status={state}")
      if st >= 200 and st < 300 and state == "ONLINE":
        online = True
        payload = pl
        break

  if not online:
    print("[WARN] 设备当前仍为离线状态")
  else:
    print("[PASS] 设备当前在线")

  if args.check_shadow:
    shadow_path = f"/v5/iot/{project_id}/devices/{device_id}/shadow"
    st, pl = request_iotda(
      base_url=base_url,
      region=region,
      instance_id=instance_id,
      ak=ak,
      sk=sk,
      method="GET",
      path_with_query=shadow_path,
      payload=None,
    )
    print(f"[INFO] GET shadow status={st}")
    print(to_json_text(pl))

  if args.send_feed:
    cmd_path = f"/v5/iot/{project_id}/devices/{device_id}/commands"
    body: dict[str, Any] = {
      "service_id": "pet_home_control",
      "command_name": "control",
      "paras": {
        "feed": True,
        "feed_target_g": int(args.feed_target),
      },
    }
    st, pl = request_iotda(
      base_url=base_url,
      region=region,
      instance_id=instance_id,
      ak=ak,
      sk=sk,
      method="POST",
      path_with_query=cmd_path,
      payload=body,
    )
    print(f"[INFO] POST command status={st}")
    print(to_json_text(pl))
    if st >= 200 and st < 300 and isinstance(pl, dict) and "response" not in pl:
      print("[NOTE] 云端仅受理命令，需后续等待设备回执事件。")

  return 0


if __name__ == "__main__":
  sys.exit(main())

