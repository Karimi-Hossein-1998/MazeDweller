# MazeDweller

A maze-dwelling game built with **SDL3** and **Dear ImGui**. Navigate the maze,
collect ExP orbs, fight pursuing enemies, and reach the golden exit.

## Platforms

| Platform | Build | Output |
|---|---|---|
| Linux (desktop) | CMake + system SDL3 (or vendored) | `MazeDweller` binary |
| Android | Gradle + AGP + NDK (see `android-project/`) | `.apk` |
| Web (WebAssembly) | Emscripten (`emcmake`/`emmake`) | `index.html` + `.js` + `.wasm` |

The game logic is identical across all three; only the entry point and the
build wrapper differ.

## Building

### Linux (desktop)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix ~/.local   # optional: install
```

### Web (Emscripten)

Install [emsdk](https://emscripten.org/docs/getting_started/downloads.html) and
activate it, then:

```bash
emcmake cmake -S . -B build-web
emmake cmake --build build-web
# serve build-web/ (e.g. `python3 -m http.server -d build-web`)
```

### Android

The SDL3 `android-project/` Gradle template builds the APK. See the project
history / `android-project/` for the full setup (requires the Android SDK +
NDK, and a `local.properties` pointing at them).

## Web deployment

Pushing to `main` triggers the `.github/workflows/deploy.yml` workflow, which
builds the WebAssembly version with Emscripten and deploys it to **GitHub
Pages**. To enable it:

1. Repo → **Settings → Pages** → Source = **GitHub Actions**.
2. Push to `main` (or run the workflow manually from the **Actions** tab).

Your live game will be at `https://<user>.github.io/MazeDweller/`.

## Controls

- **Move**: arrow keys / WASD (desktop), on-screen movepad (touch).
- **Shoot**: Space (desktop), shoot button (touch).
- **Pause**: Escape (desktop), pause button top-right (touch).
- **H**: toggle path highlight (debug).
