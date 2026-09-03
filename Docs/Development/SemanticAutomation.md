# Semantic UI and native screenshot evidence

S02-P04 extends the authenticated automation session with a stable semantic UI registry, observable-predicate synchronization and exact-size native screenshot evidence. The first surface is the development-only `AutomationProof` Slate screen.

S05-P04 adds the authoritative Lübeck placement surface and typed semantic values documented in [PlacementSemanticAutomation.md](PlacementSemanticAutomation.md).

## Stable proof IDs

| ID | Role | Actions | Notable state |
| --- | --- | --- | --- |
| `AutomationProof.Screen` | `screen` | none | root bounds and visibility |
| `AutomationProof.Panel` | `panel` | none | working-surface bounds |
| `AutomationProof.Title` | `heading` | none | stable label |
| `AutomationProof.Status` | `status` | none | `selected` after activation |
| `AutomationProof.Activate` | `button` | `activate`, `focus` | selected/focused |
| `AutomationProof.FocusTarget` | `button` | `activate`, `focus` | focused/selected |
| `AutomationProof.Warning` | `alert` | none | `warning=true` plus visible text/icon |

Semantic nodes expose `id`, `role`, `label`, `state`, integer `bounds`, `parentId`, ordered `children` and ordered `actions`. These records intentionally contain no Slate/UMG class name or traversal path.

## MCP flow

Start the game and sidecar as documented in [HansaMcp.md](HansaMcp.md). A read-only session should request `semantic-ui`, `wait-assertions` and `screenshots`; these are part of the sidecar's default requirement set. Use a startup-approved `ControlledActions` session for `ui_activate` or `ui_focus`.

The added MCP tools are:

- `ui_find` and `ui_state` with `{ semanticId }`;
- `ui_activate` and `ui_focus` with `{ semanticId }`;
- `wait_for` with `{ semanticId, property, expected?, timeoutMs? }`;
- `capture_screenshot` with either `{ width: 1280, height: 720 }` or `{ width: 1920, height: 1080 }`, plus an optional safe `bundleId`.

`wait_for` observes a semantic predicate from the endpoint ticker and returns the observed registry revision. It does not implement delays. Timeouts are monotonic, bounded to 30 seconds and returned as structured retryable `TimedOut` errors.

## Evidence bundle

Each successful capture writes:

```text
Saved/TestEvidence/Automation/S02P04/<bundle-id>/
  screenshot-<width>x<height>.png
  metadata.json
  semantic-ui.json
```

Metadata records schema version, UTC capture time, capture method, `postCaptureResized=false`, fixture, map, screen ID, width, height, UI scale, UI revision, simulation tick, frame, PNG SHA-1 and sibling filenames. The service accepts only the two approved dimensions and verifies the native buffer contains exactly `width × height` pixels before encoding it directly.

The entire `Saved` evidence tree is ignored and remains outside Shipping. Promote only deliberately reviewed documentation or baselines to tracked paths.

## Verification

```powershell
pwsh -NoProfile -File Scripts\RunHansaMcpTests.ps1
pwsh -NoProfile -File Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Architecture.Automation
```

The Unreal screenshot test encodes both supported native dimensions and checks the PNG IHDR dimensions plus metadata. A live rendered-window smoke additionally exercises the Slate proof surface through the named pipe when explicitly launched. UE's native Slate capture requires a real platform window; `-RenderOffscreen` is intentionally reported as `CaptureUnavailable` rather than replaced with synthetic evidence.
