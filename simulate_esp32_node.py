#!/usr/bin/env python3
"""Publish ESP32-like temperature and humidity telemetry to a local MQTT broker."""

import argparse
import json
import random
import time

import paho.mqtt.client as mqtt


DEFAULT_HOST = "192.168.1.81"
DEFAULT_PORT = 1883
DEFAULT_TOPIC = "nodes/ESP32_002"
DEFAULT_INTERVAL_SECONDS = 7
DEFAULT_LATITUDE = 10.890732
DEFAULT_LONGTITUDE = 106.784988


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Simulate one ESP32 telemetry node.")
    parser.add_argument("--host", default=DEFAULT_HOST, help="MQTT broker host.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="MQTT broker port.")
    parser.add_argument("--topic", default=DEFAULT_TOPIC, help="MQTT publish topic.")
    parser.add_argument(
        "--interval",
        type=float,
        default=DEFAULT_INTERVAL_SECONDS,
        help="Seconds between telemetry messages.",
    )
    parser.add_argument(
        "--temperature",
        type=float,
        default=28.5,
        help="Base simulated temperature in Celsius.",
    )
    parser.add_argument(
        "--humidity",
        type=float,
        default=65.0,
        help="Base simulated humidity percentage.",
    )
    parser.add_argument("--latitude", type=float, default=DEFAULT_LATITUDE)
    parser.add_argument(
        "--longtitude",
        type=float,
        default=DEFAULT_LONGTITUDE,
        help="Longitude field name follows task_gateway.cpp spelling.",
    )
    return parser.parse_args()


def build_payload(args: argparse.Namespace) -> str:
    payload = {
        "temperature": round(args.temperature + random.uniform(-0.4, 0.4), 1),
        "humidity": round(args.humidity + random.uniform(-1.5, 1.5), 1),
        "latitude": args.latitude,
        "longtitude": args.longtitude,
    }
    return json.dumps(payload, separators=(",", ":"))


def main() -> None:
    args = parse_args()
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id="ESP32_Node_01_Simulator",
        protocol=mqtt.MQTTv311,
    )

    print(f"Connecting to MQTT broker {args.host}:{args.port}...")
    client.connect(args.host, args.port, keepalive=60)
    client.loop_start()

    try:
        while True:
            payload = build_payload(args)
            result = client.publish(args.topic, payload, qos=0)
            result.wait_for_publish()
            print(f"Published to {args.topic}: {payload}")
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("Stopping ESP32 node simulator.")
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
