# S07-P04 inspector, alerts and causal navigation

## User flow

Select a production building or residence, inspect its state in a stable information order, follow a current problem to the shared causal explanation, take an applicable action, and return focus to the originating object or alert. Alerts group repeated conditions, expose severity/object/age/cause, and support frame, cause, snooze and pin actions without obscuring the city.

## Component inventory

| Component ID | Class | Implementation | Content and states |
| --- | --- | --- | --- |
| `Inspector.ScreenReference` | Screen reference/style anchor | Reference-only raster | HUD with alert stack and right inspector; production warning selected |
| `Inspector.Root` | Native widget | Slate | closed, production building, residence, keyboard/controller focus, localization expansion |
| `Inspector.Identity` | Native widget | Slate text/status | identity, object type, operating/construction/residence state |
| `Inspector.Result` | Native widget | Slate text/status | throughput/utilization or residents/satisfaction |
| `Inspector.Flows` | Native widget | Slate rows | inputs/outputs or needs; available/missing/fulfilled |
| `Inspector.Problem` | Native widget | Slate alert surface | none, notice, warning, critical; icon/shape/text/color redundancy |
| `Inspector.Problem.Cause` | Native widget | Slate causal-factor rows | shared-model cause, evidence, remedy and related semantic target |
| `Inspector.Actions` | Native widget | Slate buttons | enabled, disabled with reason, hover, pressed, focus |
| `Inspector.History` | Native widget | Slate rows | recent causal/state events and empty state |
| `HUD.AlertStack.Group.*` | Component reference + native widget | Reference-only raster + Slate | grouped count, maximum three expanded groups, warning/critical styling |
| `HUD.AlertStack.Alert.*` | Native widget | Slate | severity, affected object, age, cause, focused/selected/snoozed/pinned |
| alert actions | Native widget | Slate buttons | frame/open, open cause, snooze, pin/unpin |
| causal tooltip | Component reference + native widget | Reference-only raster + Slate | short and expanded text, shortcut, cause, remedy |
| focus restoration | Native behavior | Slate focus + semantic state | alert → inspector → close/back → originating alert/object |
| pinned tracker | Native widget | Slate status row | pinned/unpinned and object health summary |

No new production raster is required. Status icons, lines, focus rings, severity edges and panel surfaces use native Slate shapes/styles.

## Stable inspector order

1. identity and state;
2. most important result;
3. inputs/outputs or needs;
4. current problem and cause;
5. actions and automation;
6. historical details and flavor.

## State matrix

| Surface | Default | Hover/pressed | Selected | Disabled | Focus | Warning | Critical/loading/error |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Inspector action | Linen/slate button | native tactile tint | persistent icon/text state | muted plus reason tooltip | Brass ring and focus label | Amber icon/text where risky | Oxblood icon/text for destructive/error |
| Alert row | Severity edge, icon, title/object/age/cause | native button feedback | Brass outline | snoozed label and reduced emphasis | Brass ring independent of hover | Amber triangle + “Warning” | Oxblood octagon + “Critical”; stable geometry while loading |
| Causal factor | Ink ledger row | related target affordance | selected causal row | explicit unavailable reason | Brass rule | warning glyph + factor text | error glyph + cause/remedy text |
| Pinned tracker | icon + object/result | native button feedback | pin glyph + “Pinned” | unavailable explanation | Brass ring | warning summary | critical summary |

All interactive targets are at least 40×40 px and the controller path uses 48 px targets. Labels reserve approximately 35% expansion for German and other long localizations. No status depends on color alone.

## Reference generation plan

1. A 16:9 composed `ui-mockup` showing the alert-to-cause inspector hierarchy.
2. One isolated reusable inspector-panel reference in production-building warning state.
3. One isolated grouped-alert and causal-tooltip family reference.

Each reference uses the main city HUD as the family style anchor, is saved at its generator-native dimensions without resampling, receives a sibling prompt record, and is inspected at original resolution. Shipping UI is reconstructed natively.
