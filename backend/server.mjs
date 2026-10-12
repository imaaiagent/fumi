import http from "node:http";

const PORT = Number(process.env.PORT || 8787);
const PROVIDER = (process.env.LLM_PROVIDER || "openai").toLowerCase();
const MODEL = process.env.LLM_MODEL || "";
const SYSTEM_PROMPT = process.env.SYSTEM_PROMPT || "You are FUMI, a friendly, curious, concise little AI companion.";
const MAX_JSON_BYTES = Number(process.env.MAX_JSON_BYTES || 32768);
const MAX_AUDIO_BYTES = Number(process.env.MAX_AUDIO_BYTES || 8_000_000);

const providers = {
  openai: { baseURL: "https://api.openai.com/v1", key: () => process.env.OPENAI_API_KEY },
  xai: { baseURL: "https://api.x.ai/v1", key: () => process.env.XAI_API_KEY },
  openrouter: { baseURL: "https://openrouter.ai/api/v1", key: () => process.env.OPENROUTER_API_KEY },
  ollama: { baseURL: process.env.LLM_BASE_URL || "http://127.0.0.1:11434/v1", key: () => process.env.LLM_API_KEY || "ollama" },
  "openai-compatible": { baseURL: process.env.LLM_BASE_URL, key: () => process.env.LLM_API_KEY },
};

function send(res, status, body) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" });
  res.end(JSON.stringify(body));
}
function cors(req, res) {
  res.setHeader("access-control-allow-origin", process.env.CORS_ORIGIN || "*");
  res.setHeader("access-control-allow-methods", "GET,POST,OPTIONS");
  res.setHeader("access-control-allow-headers", "content-type,authorization,x-fumi-device-id");
  if (req.method === "OPTIONS") { res.writeHead(204); res.end(); return true; }
  return false;
}
async function readBody(req, limit) {
  const chunks = []; let total = 0;
  for await (const chunk of req) {
    total += chunk.length;
    if (total > limit) { const e = new Error("Request body too large"); e.status = 413; throw e; }
    chunks.push(chunk);
  }
  return Buffer.concat(chunks);
}
async function readJson(req) {
  const raw = await readBody(req, MAX_JSON_BYTES);
  try { return JSON.parse(raw.toString("utf8") || "{}"); }
  catch { const e = new Error("Invalid JSON body"); e.status = 400; throw e; }
}

async function chat(message) {
  if (!MODEL) throw new Error("Set LLM_MODEL to a model ID supported by your provider.");
  if (PROVIDER === "anthropic") {
    const key = process.env.ANTHROPIC_API_KEY;
    if (!key) throw new Error("Missing ANTHROPIC_API_KEY.");
    const r = await fetch("https://api.anthropic.com/v1/messages", {
      method: "POST",
      headers: { "content-type": "application/json", "x-api-key": key, "anthropic-version": "2023-06-01" },
      body: JSON.stringify({ model: MODEL, max_tokens: 700, system: SYSTEM_PROMPT, messages: [{ role: "user", content: message }] }),
      signal: AbortSignal.timeout(60000),
    });
    const d = await r.json().catch(() => ({}));
    if (!r.ok) throw new Error(`Anthropic API ${r.status}: ${d.error?.message || "request failed"}`);
    return (d.content || []).filter(x => x.type === "text").map(x => x.text).join("\n").trim();
  }
  const p = providers[PROVIDER];
  if (!p) throw new Error(`Unsupported LLM_PROVIDER "${PROVIDER}".`);
  if (!p.baseURL) throw new Error("Set LLM_BASE_URL for openai-compatible.");
  const key = p.key();
  if (PROVIDER !== "ollama" && !key) throw new Error(`Missing API key for provider "${PROVIDER}".`);
  const headers = { "content-type": "application/json" };
  if (key) headers.authorization = `Bearer ${key}`;
  if (PROVIDER === "openrouter") {
    headers["http-referer"] = process.env.PUBLIC_APP_URL || "http://localhost";
    headers["x-title"] = "FUMI";
  }
  const r = await fetch(`${p.baseURL.replace(/\/+$/, "")}/chat/completions`, {
    method: "POST", headers,
    body: JSON.stringify({ model: MODEL, messages: [{ role: "system", content: SYSTEM_PROMPT }, { role: "user", content: message }], temperature: 0.7, max_tokens: 700 }),
    signal: AbortSignal.timeout(60000),
  });
  const d = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(`${PROVIDER} API ${r.status}: ${d.error?.message || d.message || "request failed"}`);
  const content = d.choices?.[0]?.message?.content;
  if (typeof content === "string") return content.trim();
  if (Array.isArray(content)) return content.map(x => x.text || "").join("\n").trim();
  throw new Error("Provider returned no text content.");
}

async function transcribe(audio, contentType) {
  if ((process.env.VOICE_PROVIDER || "openai").toLowerCase() !== "openai") {
    throw new Error("This reference build currently implements VOICE_PROVIDER=openai only.");
  }
  const key = process.env.OPENAI_API_KEY;
  if (!key) throw new Error("Voice transcription requires OPENAI_API_KEY.");
  const form = new FormData();
  form.append("file", new Blob([audio], { type: contentType?.includes("wav") ? "audio/wav" : "application/octet-stream" }), "fumi.wav");
  form.append("model", process.env.VOICE_MODEL || "whisper-1");
  const r = await fetch("https://api.openai.com/v1/audio/transcriptions", {
    method: "POST", headers: { authorization: `Bearer ${key}` }, body: form, signal: AbortSignal.timeout(90000),
  });
  const d = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(`Speech-to-text API ${r.status}: ${d.error?.message || "request failed"}`);
  return String(d.text || "").trim();
}

const server = http.createServer(async (req, res) => {
  if (cors(req, res)) return;
  const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);
  if (req.method === "GET" && url.pathname === "/health") {
    return send(res, 200, { ok: true, service: "fumi-brain", provider: PROVIDER, model: MODEL || null });
  }
  if (req.method === "POST" && url.pathname === "/brain") {
    try {
      const b = await readJson(req);
      const message = String(b.message ?? b.text ?? b.prompt ?? b.input ?? "").trim();
      if (!message) return send(res, 400, { ok: false, error: "Send a non-empty message, text, prompt, or input field." });
      const reply = await chat(message);
      return send(res, 200, { ok: true, reply, action: "talk", expression: "normal", provider: PROVIDER, model: MODEL });
    } catch (e) {
      console.error("[/brain]", e.message);
      return send(res, e.status || 502, { ok: false, error: e.message || "Brain request failed" });
    }
  }
  if (req.method === "POST" && url.pathname === "/voice") {
    try {
      const audio = await readBody(req, MAX_AUDIO_BYTES);
      if (!audio.length) return send(res, 400, { ok: false, error: "Empty audio upload." });
      const transcript = await transcribe(audio, req.headers["content-type"] || "");
      if (!transcript) return send(res, 422, { ok: false, error: "No speech detected.", transcript: "" });
      const reply = await chat(transcript);
      return send(res, 200, { ok: true, transcript, reply, action: "talk", expression: "normal", provider: PROVIDER, model: MODEL });
    } catch (e) {
      console.error("[/voice]", e.message);
      return send(res, e.status || 502, { ok: false, error: e.message || "Voice request failed" });
    }
  }
  return send(res, 404, { ok: false, error: "Not found", routes: ["GET /health", "POST /brain", "POST /voice"] });
});
server.listen(PORT, "0.0.0.0", () => console.log(`FUMI brain listening on ${PORT}; provider=${PROVIDER}; model=${MODEL || "(unset)"}`));
