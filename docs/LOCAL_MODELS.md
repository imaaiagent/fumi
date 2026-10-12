# Local / offline model support

FUMI's text backend can use Ollama through its OpenAI-compatible API.

Example:

```env
LLM_PROVIDER=ollama
LLM_BASE_URL=http://127.0.0.1:11434/v1
LLM_MODEL=your-installed-model
```

For a fully local deployment:

```text
FUMI
  │
  ▼
Local FUMI backend
  │
  ▼
Ollama
  │
  ▼
Local language model
```

The ESP32 still needs network access to reach the machine hosting the backend. The language model itself does not need a cloud API.

A truly offline-on-device LLM is a separate hardware/quantization project and is not implied by this backend mode.
