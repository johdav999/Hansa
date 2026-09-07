# S12-P02 Authoring Studio generation jobs

The shipping interface is assembled from native Slate widgets. Generated images in this folder are visual references only and are never imported into `Content/`.

## Component inventory

| Area | Component | Native implementation | Required states |
| --- | --- | --- | --- |
| Screen shell | Existing Hansa Authoring Studio frame | Existing Slate border, toolbar, and workspace switcher | default, keyboard/controller focus |
| Navigation | Generation workspace tab | `SButton` in the Studio toolbar | default, hover, pressed, selected, disabled, focus |
| Provider panel | Connection and capability summary | `SBorder`, `STextBlock`, `SComboBox` | disconnected, loading, ready, warning, error, focus |
| Submission panel | Prompt, provider/model/capability, role, budget, timeout | Slate text and numeric controls | default, edited, disabled, focus, warning, error |
| Upload preview | Exact list of files that will leave the machine | `SListView` with path, role, media type, byte count, and SHA-256 | empty, populated, selected, hover, focus, invalid, loading |
| Approval controls | Rights acknowledgement and estimated spend confirmation | `SCheckBox` plus explicit queue button | unchecked, checked, disabled, focus, warning, error |
| Job queue | Virtualized list of worker jobs | `SListView` rows with status, progress, cost, and actions | queued, running, cancelling, cancelled, failed, retrying, review, selected, hover, focus |
| Status and feedback | Connection, validation, progress, and structured worker errors | Semantic text, progress bar, native icons | information, loading, success, warning, error |
| Result panel | Output artifact and provenance inspection | Read-only Slate list and text panels | empty, loading, review-ready, selected, warning, error |
| Actions | Refresh, add input, remove input, queue, cancel, retry | Native buttons with tooltips | default, hover, pressed, disabled, focus |
| Charts and overlays | None | Not applicable | Not applicable |
| Icons | Unreal Editor/AppStyle icons and semantic glyphs | Native brushes/text | default, disabled, focus |
| Decorative imagery | None | Not applicable | Not applicable |

## Layout and behavior

- The generation workspace uses a three-column layout: request and approvals, virtualized job queue, and selected result/provenance.
- The exact upload preview is shown before any queue action and includes every outbound byte source. A request with files cannot be queued unless the operator acknowledges the rights declaration.
- Estimated currency and maximum spend appear next to the confirmation control. Submission and retry each require a fresh approval.
- Job state is communicated by text and icon as well as color. Cancel and retry appear only when the worker state permits the action.
- Dynamic text, amounts, paths, hashes, provider names, and statuses are always native Slate content.
- Hansa semantic colors use the tokens from `Docs/UIDesignBrief.md`; the rest of the shell follows Unreal Editor styling.
- The layout reserves space for translated labels and supports keyboard focus for every interactive control.

