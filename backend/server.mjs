import http from "node:http";

const PORT = Number(process.env.PORT || 8787);
const PROVIDER = (process.env.LLM_PROVIDER || "openai").toLowerCase();
const MODEL = process.env.LLM_MODEL || "";
const SYSTEM_PROMPT = process.env.SYSTEM_PROMPT || "You are FUMI, a friendly, curious, playful AI pet. Be concise and speak naturally.";
const MAX_JSON_BYTES = Number(process.env.MAX_JSON_BYTES || 32768);
const MAX_AUDIO_BYTES = Number(process.env.MAX_AUDIO_BYTES || 8_000_000);
const DEVICE_TOKEN = process.env.FUMI_DEVICE_TOKEN || "";
const memories = new Map();
const voiceMemories = new Map();

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
  const origin = process.env.CORS_ORIGIN || "*";
  res.setHeader("access-control-allow-origin", origin);
  res.setHeader("vary", "Origin");
  res.setHeader("access-control-allow-methods", "GET,POST,OPTIONS");
  res.setHeader("access-control-allow-headers", "content-type,authorization,x-fumi-device-id,x-fumi-device-token");
  if (req.method === "OPTIONS") { res.writeHead(204); res.end(); return true; }
  return false;
}
function authorized(req) {
  if (!DEVICE_TOKEN) return true;
  const supplied = req.headers["x-fumi-device-token"];
  return typeof supplied === "string" && supplied.length > 0 && supplied === DEVICE_TOKEN;
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
function clamp(n, min, max) {
  const value = Number(n);
  return Math.max(min, Math.min(max, Number.isFinite(value) ? value : 0));
}
function cleanMessage(value, max = 180) {
  return String(value ?? "").replace(/\s+/g, " ").replace(/"/g, "").trim().slice(0, max);
}
function parseModelJson(text) {
  const cleaned = String(text || "").trim()
    .replace(/^```json\s*/i, "").replace(/^```\s*/i, "").replace(/\s*```$/i, "").trim();
  try { return JSON.parse(cleaned); } catch { return null; }
}

async function requestText(userInput, systemPrompt = SYSTEM_PROMPT, maxTokens = 700) {
  if (!MODEL) throw new Error("Set LLM_MODEL to a model ID supported by your provider.");
  if (PROVIDER === "anthropic") {
    const key = process.env.ANTHROPIC_API_KEY;
    if (!key) throw new Error("Missing ANTHROPIC_API_KEY.");
    const r = await fetch("https://api.anthropic.com/v1/messages", {
      method: "POST",
      headers: { "content-type": "application/json", "x-api-key": key, "anthropic-version": "2023-06-01" },
      body: JSON.stringify({ model: MODEL, max_tokens: maxTokens, system: systemPrompt, messages: [{ role: "user", content: userInput }] }),
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
    body: JSON.stringify({
      model: MODEL,
      messages: [{ role: "system", content: systemPrompt }, { role: "user", content: userInput }],
      temperature: 0.7,
      max_tokens: maxTokens
    }),
    signal: AbortSignal.timeout(60000),
  });
  const d = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(`${PROVIDER} API ${r.status}: ${d.error?.message || d.message || "request failed"}`);
  const content = d.choices?.[0]?.message?.content;
  if (typeof content === "string") return content.trim();
  if (Array.isArray(content)) return content.map(x => x.text || "").join("\n").trim();
  throw new Error("Provider returned no text content.");
}

function buildBrainPrompt(state, memory) {
  const day = Math.floor((Number(state.age_seconds) || 0) / 86400) + 1;
  return `You are deciding FUMI's next safe behavior. Return ONLY JSON, with no markdown.
FUMI is a playful, curious, affectionate tiny creature living inside an ESP32 device.

Current state:
- day: ${day}
- hunger: ${clamp(state.hunger, 0, 100)}
- happiness: ${clamp(state.happiness, 0, 100)}
- energy: ${clamp(state.energy, 0, 100)}
- bond: ${clamp(state.bond, 0, 100)}
- affection: ${clamp(state.affection, 0, 100)}
- playfulness: ${clamp(state.playfulness, 0, 80)}
- sleeping: ${Boolean(state.sleeping)}
- current expression: ${state.expression ?? "normal"}
- event: ${state.event || "heartbeat"}

Recent memory:
${memory.length ? memory.map(x => "- " + x).join("\n") : "- first day"}

Choose exactly one action: idle, talk, play, ask_food, sleep.
Choose exactly one expression: normal, happy, surprised, sleepy, hungry, very_hungry, sleep, sad.
Rules:
- If hunger > 80, strongly prefer ask_food and hungry/very_hungry.
- If energy <= 20, prefer sleep/sleepy.
- If happiness < 30, consider sad.
- If event is feed, tap, or game_finished, react to it.
- Never invent hardware actions.
- message is a tiny FUMI-style line, maximum 90 characters.
- JSON shape: {"action":"talk","expression":"happy","message":"hey there!"}`;
}

async function transcribe(audio, contentType) {
  const provider = (process.env.VOICE_PROVIDER || "openai").toLowerCase();
  if (provider === "deepgram") {
    const key = process.env.DEEPGRAM_API_KEY;
    if (!key) throw new Error("Missing DEEPGRAM_API_KEY.");
    const url = new URL("https://api.deepgram.com/v1/listen");
    url.searchParams.set("model", process.env.VOICE_MODEL || "nova-3");
    url.searchParams.set("smart_format", "true");
    const r = await fetch(url, {
      method: "POST",
      headers: { authorization: `Token ${key}`, "content-type": contentType?.includes("wav") ? "audio/wav" : (contentType || "application/octet-stream") },
      body: audio, signal: AbortSignal.timeout(90000),
    });
    const d = await r.json().catch(() => ({}));
    if (!r.ok) throw new Error(`Deepgram API ${r.status}: ${d.message || "request failed"}`);
    return String(d.results?.channels?.[0]?.alternatives?.[0]?.transcript || "").trim();
  }
  if (provider === "groq") {
    const key = process.env.GROQ_API_KEY;
    if (!key) throw new Error("Missing GROQ_API_KEY.");
    const form = new FormData();
    form.append("file", new Blob([audio], { type: contentType?.includes("wav") ? "audio/wav" : "application/octet-stream" }), "fumi.wav");
    form.append("model", process.env.VOICE_MODEL || "whisper-large-v3");
    const r = await fetch("https://api.groq.com/openai/v1/audio/transcriptions", {
      method: "POST", headers: { authorization: `Bearer ${key}` }, body: form, signal: AbortSignal.timeout(90000),
    });
    const d = await r.json().catch(() => ({}));
    if (!r.ok) throw new Error(`Groq API ${r.status}: ${d.error?.message || "request failed"}`);
    return String(d.text || "").trim();
  }
  if (provider !== "openai") throw new Error(`Unsupported VOICE_PROVIDER "${provider}".`);
  const key = process.env.OPENAI_API_KEY;
  if (!key) throw new Error("Missing OPENAI_API_KEY.");
  const form = new FormData();
  form.append("file", new Blob([audio], { type: contentType?.includes("wav") ? "audio/wav" : "application/octet-stream" }), "fumi.wav");
  form.append("model", process.env.VOICE_MODEL || "whisper-1");
  const r = await fetch("https://api.openai.com/v1/audio/transcriptions", {
    method: "POST", headers: { authorization: `Bearer ${key}` }, body: form, signal: AbortSignal.timeout(90000),
  });
  const d = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(`OpenAI speech API ${r.status}: ${d.error?.message || "request failed"}`);
  return String(d.text || "").trim();
}

const server = http.createServer(async (req, res) => {
  if (cors(req, res)) return;
  const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);

  if (req.method === "GET" && (url.pathname === "/" || url.pathname === "/health")) {
    return send(res, 200, { ok: true, name: "FUMI AI Brain", status: "online", provider: PROVIDER, model: MODEL || null, endpoints: ["GET /health", "POST /brain", "POST /voice"] });
  }

  if ((url.pathname === "/brain" || url.pathname === "/voice") && !authorized(req)) {
    return send(res, 401, { ok: false, error: "Unauthorized FUMI device." });
  }

  if (req.method === "POST" && url.pathname === "/brain") {
    try {
      const state = await readJson(req);
      const deviceId = String(state.device_id || req.headers["x-fumi-device-id"] || "fumi-main").slice(0, 80);

      // Firmware sends life-state events; the browser demo may send a plain message.
      if (state.message || state.text || state.prompt || state.input) {
        const message = String(state.message ?? state.text ?? state.prompt ?? state.input).trim().slice(0, 2000);
        if (!message) return send(res, 400, { ok: false, error: "Message cannot be empty." });
        const reply = await requestText(message, SYSTEM_PROMPT, 500);
        return send(res, 200, { ok: true, action: "talk", expression: "normal", message: cleanMessage(reply, 500), reply, provider: PROVIDER, model: MODEL });
      }

      const previous = memories.get(deviceId) || [];
      const raw = await requestText(buildBrainPrompt(state, previous), "You are FUMI's behavior engine. Follow the requested JSON schema exactly.", 250);
      let result = parseModelJson(raw) || { action: "talk", expression: "normal", message: "hmm... give me a second" };
      const allowedActions = ["idle", "talk", "play", "ask_food", "sleep"];
      const allowedExpressions = ["normal", "happy", "surprised", "sleepy", "hungry", "very_hungry", "sleep", "sad"];
      if (!allowedActions.includes(result.action)) result.action = "idle";
      if (!allowedExpressions.includes(result.expression)) result.expression = "normal";
      result.message = cleanMessage(result.message, 90) || "hey...";
      memories.set(deviceId, [...previous, `${state.event || "heartbeat"} -> ${result.action}/${result.expression}: ${result.message}`].slice(-12));
      return send(res, 200, { ok: true, action: result.action, expression: result.expression, message: result.message, provider: PROVIDER, model: MODEL });
    } catch (e) {
      console.error("[/brain]", e.message);
      return send(res, e.status || 502, { ok: false, error: e.message || "Brain request failed", action: "idle", expression: "normal", message: "my brain is sleepy..." });
    }
  }

  if (req.method === "POST" && url.pathname === "/voice") {
    try {
      const audio = await readBody(req, MAX_AUDIO_BYTES);
      if (audio.length < 44) return send(res, 400, { ok: false, error: "Audio body is empty or too small." });
      if (audio.toString("ascii", 0, 4) !== "RIFF" || audio.toString("ascii", 8, 12) !== "WAVE") {
        return send(res, 400, { ok: false, error: "Expected a WAV file (RIFF/WAVE)." });
      }
      const transcript = await transcribe(audio, req.headers["content-type"] || "");
      if (!transcript) return send(res, 422, { ok: false, error: "No speech detected.", transcript: "", reply: "Sorry, I didn't catch that. Try again?" });
      const deviceId = String(req.headers["x-fumi-device-id"] || "fumi-main").slice(0, 80);
      const previous = voiceMemories.get(deviceId) || [];
      const system = `${SYSTEM_PROMPT} Speak naturally as FUMI in English. Keep answers to 1–3 short sentences for a small speaker. Do not output JSON, markdown, or stage directions. Recent conversation:\n${previous.join("\n") || "- Start of conversation."}`;
      const reply = (await requestText(`User says: ${transcript}\nReply as FUMI.`, system, 500)).replace(/\s+/g, " ").trim().slice(0, 500);
      voiceMemories.set(deviceId, [...previous, `User: ${transcript}`, `FUMI: ${reply}`].slice(-12));
      return send(res, 200, { ok: true, transcript, reply, action: "talk", expression: "normal", provider: PROVIDER, model: MODEL });
    } catch (e) {
      console.error("[/voice]", e.message);
      return send(res, e.status || 502, { ok: false, error: e.message || "Voice request failed" });
    }
  }
  return send(res, 404, { ok: false, error: "Not found", routes: ["GET /health", "POST /brain", "POST /voice"] });
});
server.listen(PORT, "0.0.0.0", () => console.log(`FUMI brain listening on ${PORT}; provider=${PROVIDER}; model=${MODEL || "(unset)"}`));
