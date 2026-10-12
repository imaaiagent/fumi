# Releases

FUMI uses Git tags for public releases.

Example:

```bash
git tag v0.3.0
git push origin v0.3.0
```

The GitHub Actions release workflow creates a GitHub Release and attaches a source archive.

For hardware builds, keep the exact Arduino IDE, ESP32 core, library versions, and board settings in the release notes.
