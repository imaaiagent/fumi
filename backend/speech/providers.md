# Supported speech providers

| Provider | Environment | Typical use |
|---|---|---|
| OpenAI | `VOICE_PROVIDER=openai` | Hosted transcription |
| Groq | `VOICE_PROVIDER=groq` | Whisper-compatible hosted transcription |
| Deepgram | `VOICE_PROVIDER=deepgram` | Hosted speech-to-text |

The firmware only uploads WAV audio to `/voice`; it does not need to know which speech provider is selected.
