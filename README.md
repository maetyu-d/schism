# Schism

![](https://github.com/maetyu-d/schism/blob/main/Angus_Wright_as_Angus_Maynard.jpeg)

> "There is no sin, no amount of evil, which should be permitted to dissolve or break the bond of unity. For love can do all things, and nothing is difficult to those who are united." - Angus Maynard, after Martin Luther (1519)

Part in-joke, part functional prototype, Schism is a dual-view DSP environment developed in JUCE, aimed at people who love Pd while their friends love SuperCollider (or vice versa). Its central premise is that it provides two simultaneous, real-time views of a single, unified internal model -and thereby permanently heals the visual-versus-text audio language Great Schism. On the left is a visual graph (a Pd-style patch canvas); on the right is a text-based graph language editor with SC-inspired syntax. Peace is finally achieved.

## What is implemented

- Canonical Graph IR (`nodes`, `edges`, `bindings`)
- Text frontend:
  - Lexer + parser for a deterministic graph language subset
  - AST-to-IR compilation
  - Basic pretty-printer
- Visual frontend:
  - Freeform Pd-style canvas with draggable boxes and cable connections
  - `Put` popup-style creation on right-click (`obj`, `msg`, `floatatom` + quick DSP objects)
  - Inline Pd-style text entry immediately after placing `obj/msg/floatatom`
  - Double-click `obj/msg/floatatom` to edit text in-canvas
  - Rewire by dragging output port to input port
  - Pd-like marquee multi-select and group drag
  - Visual add/delete actions mutate the same canonical IR
  - Zoom + pan in patch canvas (`mouse wheel` zoom, `right/middle-drag` pan)
  - Draggable divider to resize patch/code split
  - Save/load complete patches as `.schism` (text + graph + layout + split)
- Sync engine:
  - NodeID <-> text span mapping
  - Selection sync both directions
  - Stable NodeID preservation by binding hints + structural matching
- Runtime:
  - Basic block processing for ops (`constant`, `sin`, `saw`, `square`, `noise`, `+`, `-`, `*`, `/`, `lpf`, `hpf`, `delay1`, `scope`, `spectrum`, `out`)
  - Hot-swap graph updates with short output crossfade
  - Best-effort per-node state transfer (matched by stable NodeID/op)
  - Built-in scope + spectrum widgets fed from runtime output probe
- Editing:
  - Undo/Redo snapshots for both text and visual edits
  - Keyboard shortcuts: `Cmd+Z` undo, `Cmd+Shift+Z` redo (`Cmd+Y` also redo)
  - Text-edit history coalescing to avoid one-undo-per-keystroke
  - Cycle policy enforcement: feedback loops must pass through `delay1`
  - Typed connection validation blocks incompatible cables at edit time
  - Probe target selectors for `scope(nodeX)` and `spectrum(nodeY)` taps

## Build

If you already have a JUCE checkout on disk:

```bash
cmake -S . -B build -DJUCE_DIR=/absolute/path/to/JUCE
cmake --build build
```

If you do not pass `JUCE_DIR`, CMake fetches JUCE via `FetchContent`.

## Headless Self-Test

This project includes a non-GUI self-test executable to validate core behavior in environments where AppKit windows are unavailable.

Build and run:

```bash
cmake -S . -B build-local -DJUCE_DIR=/absolute/path/to/JUCE
cmake --build build-local -j4
./build-local/SchismSelfTest_artefacts/SchismSelfTest --headless-selftest
```

Formats and filtering:

```bash
./build-local/SchismSelfTest_artefacts/SchismSelfTest --headless-selftest --format=json
./build-local/SchismSelfTest_artefacts/SchismSelfTest --headless-selftest --only=stable_node_ids
```

## Language examples

```txt
osc = sin(220);
amp = 0.2;
sig = (osc * amp) -> lpf(1200);
out(0, sig);
```

Supported syntax includes:

- Assignments: `name = expr;`
- Calls: `sin(220)`, `lpf(sig, 1200)`
- Pipeline: `sig -> lpf(1200)`
- Operators: `+ - * /`
- Control annotation: `@k knob`

## Notes

- This is still an MVP scaffold focused on deterministic dual projection architecture.
- Canvas currently supports one output jack and N input jacks per node; richer typed-port UI can build on top of this.
