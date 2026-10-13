import { spawn } from "node:child_process";
import assert from "node:assert/strict";

const port = 18787;
const child = spawn(process.execPath, ["--env-file=.env", "server.mjs"], {
  cwd: new URL(".", import.meta.url),
  env: { ...process.env, PORT: String(port), FUMI_DEVICE_TOKEN: "test-token", LLM_MODEL: "unused" },
  stdio: ["ignore", "inherit", "inherit"],
});
try {
  let ready = false;
  for (let i = 0; i < 40; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${port}/health`);
      if (r.ok) { ready = true; break; }
    } catch {}
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  assert.equal(ready, true, "health endpoint should start");
  const health = await fetch(`http://127.0.0.1:${port}/health`);
  assert.equal(health.status, 200);
  const unauthorized = await fetch(`http://127.0.0.1:${port}/brain`, {
    method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ event: "heartbeat" })
  });
  assert.equal(unauthorized.status, 401, "brain route should enforce configured device token");
  const authorizedButNoKey = await fetch(`http://127.0.0.1:${port}/brain`, {
    method: "POST",
    headers: { "content-type": "application/json", "x-fumi-device-token": "test-token" },
    body: JSON.stringify({ event: "heartbeat" })
  });
  assert.notEqual(authorizedButNoKey.status, 401, "valid token should pass authentication");
  console.log("Smoke tests passed: health, auth reject, auth accept.");
} finally {
  child.kill("SIGTERM");
}
