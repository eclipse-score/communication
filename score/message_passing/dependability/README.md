# `dependability/` conventions

This directory holds the frozen, authoritative TRLC/PlantUML artifacts for the `message_passing`
`dependable_element`. If content here and anything under `research/` ever disagree, this directory
wins — the disagreement is a finding for `research/backlog.md`, not license to trust `research/`
instead.

## Keep these files free of process/meta-information

Do not put "why this was written this way", pointers to a specific `research/changes/<cycle>/`
directory, or other authoring-process narrative inside `.trlc`/`.puml` files. That belongs in
`research/` (`problem_statement.md`, `changes/<cycle>/`). Records here should read as pure,
timeless black-box/technical content, not a log of how they came to be.

## Assumed System Requirements writing convention

Each `AssumedSystemReq` should state a black-box capability an integrator evaluating
`message_passing` as a product would check for — e.g. core communication model, happy-flow
interaction patterns, production-error detectability, integration-time misconfiguration
detectability, platform support, access-control support — not how the component behaves
internally under failure, and not any specific interface method.
