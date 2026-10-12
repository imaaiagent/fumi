# FUMI API

The reference backend exposes two main device-facing endpoints.

## `GET /health`

```bash
curl http://localhost:8787/health
```

## `POST /brain`

Accepts text input or a FUMI state event.

```json
{
  "message": "Hey FUMI, introduce yourself."
}
```

A device can also send state:

```json
{
  "event": "idle",
  "hunger": 20,
  "happiness": 80,
  "energy": 90,
  "bond": 35,
  "affection": 40,
  "playfulness": 70,
  "sleeping": false,
  "expression": "happy",
  "age_seconds": 3600
}
```

The response includes generated message and FUMI action/expression information.

## `POST /voice`

Accepts raw WAV audio. The backend sends it to the configured speech-to-text provider, then sends the transcript to the configured text-model provider.

```text
microphone -> WAV -> speech-to-text -> transcript -> text model -> FUMI
```

The speech and text providers are intentionally separate.

## Authentication

When `FUMI_DEVICE_TOKEN` is configured, device requests use:

```text
X-Fumi-Device-Token: <token>
```

See `AUTHENTICATION.md`.

## Limits

Configure request limits with `MAX_JSON_BYTES` and `MAX_AUDIO_BYTES`.

Do not expose a production backend without authentication and appropriate network controls.
