# FUMI Architecture

```text
ESP32-S3 / LVGL
    │
    ├── touch
    ├── display
    ├── life state
    ├── mini-game
    ├── microphone
    │
    └── HTTPS
          │
          ▼
      FUMI backend
       ├── /brain
       │      └── selected text model
       │             ├── OpenAI
       │             ├── Claude
       │             ├── Grok
       │             ├── OpenRouter
       │             └── compatible APIs
       │
       └── /voice
              └── speech-to-text
                    └── selected text model
```

The firmware does not need to know which text model is being used. Provider selection belongs to the backend.

This separation keeps provider API keys off the physical device and lets a user switch models without reflashing the ESP32.
