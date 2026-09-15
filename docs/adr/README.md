# Architecture Decision Records

Short records of notable decisions about Pomona — the hydroponics tower, its
GIGA unit and the cluster side of its telemetry. One file per decision,
`NNNN-kebab-title.md`, Michael-Nygard style (Context → Decision →
Consequences), the same convention as the demeter, jupiter and zeus repos.
Status: Proposed / Accepted / Superseded.

Decisions that belong to the autodosing controller live in the
[demeter ADR series](https://github.com/jellebens/demeter/tree/master/docs/adr);
cross-cutting cluster decisions in the gitops `docs/adr/` series. Earlier
Pomona decisions are recorded narratively in [`../design.md`](../design.md)
and [`../control-architecture.md`](../control-architecture.md).

| # | Title | Status |
|---|-------|--------|
| [0001](0001-telemetry-archived-forever.md) | Every Pomona topic is archived in InfluxDB forever — sensors, the control plane and Demeter's stream, verbatim | Accepted |
