# Optional FUMI device authentication

The backend supports an optional shared device token for `/brain` and `/voice`.

1. Set a long random value in the backend environment:

```env
FUMI_DEVICE_TOKEN=replace-with-a-long-random-token
```

2. Set the same value in the private firmware config:

```cpp
#define FUMI_DEVICE_TOKEN_CONFIG "replace-with-the-same-token"
```

3. Rebuild and flash the firmware.

When configured, firmware sends `X-Fumi-Device-Token` with both `/brain` and `/voice` requests. Leave the backend token empty for local development.

**Security note:** a shared token embedded in firmware is a basic access gate, not strong per-device identity. Do not reuse provider API keys as device tokens. For a public deployment, also enable HTTPS, rate limits, quotas, and token rotation.
