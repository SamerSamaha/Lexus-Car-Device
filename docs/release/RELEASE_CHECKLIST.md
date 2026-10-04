# Release checklist

`dev` is promoted to `main` only through a release pull request, and only after every item below is checked and recorded in that pull request's description. The release is tagged on `main` with an annotated tag `vX.Y.Z`. Planned releases: v0.1.0 (sprint 1), v0.2.0 (sprint 2), v1.0.0 (sprint 3).

The pull request into `main` uses a merge commit (the `main` ruleset allows only that), so `main` keeps the exact squash commits that were reviewed on `dev`.

## Before opening the release pull request

### Code and tests
- [ ] CI is green on the head of `dev`: Build and unit tests, Static analysis, Privacy check.
- [ ] Every ticket in the sprint is Done on the board or explicitly carried over, with its Actual hours entered.
- [ ] `ctest --preset sanitizers` and `ctest --preset release` pass at the desk on a clean build folder; the counts are pasted in the pull request.
- [ ] No test is disabled or skipped without a ticket number in the skip reason.

### Requirements and traceability
- [ ] `docs/requirements/REQUIREMENTS.md` matches what the code does. Any requirement whose behaviour changed in the sprint has its row updated.
- [ ] `docs/traceability/TRACEABILITY.md` is current: every requirement implemented in this release points at a test that exists and is tagged; the coverage summary numbers are recomputed.
- [ ] Every design note for a merged ticket has its "Design vs. implementation" section filled in and status Implemented.

### Measurements
- [ ] Every performance or resource claim made in the release notes has its CSV and method note under `docs/measurements/`, with sample size stated.
- [ ] Thermal and power logs of every Pi session in the sprint are committed; any threshold fail has a bug issue linked.
- [ ] Nothing was rounded, dropped or re-run until it passed. Bad runs are in the data.

### Safety and privacy
- [ ] `tools/check_private_data.py` reports 0 findings on the release commit.
- [ ] No VIN, no Bluetooth address, no unscrubbed recording anywhere in the tree, including `docs/measurements/` and `docs/test/results/`.
- [ ] The allowlist test (REQ-001) passed in this release's CI run; nothing added in the sprint sends toward the vehicle outside the allowlist.
- [ ] `docs/safety/SAFETY_STATEMENT.md` still describes what the software does; updated if a hazard or mitigation changed.

### Documentation
- [ ] `README.md` status section says which release this is and what works.
- [ ] `docs/architecture/ARCHITECTURE.md` matches the merged design notes; "Not decided yet" updated.
- [ ] The plan (`docs/planning/KICKOFF_PLAN.md`) has the sprint's actual hours against estimates and the next sprint's tickets.
- [ ] Release notes drafted: what works, what is measured (with numbers), what is known not to work, what changed since the previous release.

### Repository hygiene
- [ ] Every merged feature branch is deleted on GitHub.
- [ ] No file in the tree is excluded from the public repository by mistake, and nothing private is included (compare `git ls-files` against the layout in the plan).
- [ ] The version in the top-level `CMakeLists.txt` (`project(... VERSION X.Y.Z)`) equals the tag to be created, and the version string returned by the service library equals it.

## The release pull request
- [ ] Title: `Release vX.Y.Z`. Base `main`, head `dev`.
- [ ] Body: this checklist with every box ticked and the evidence (commit hashes, test counts, CI run links) beside each item; the release notes.
- [ ] Required status checks green on the pull request.
- [ ] Written review posted as a comment, same template as any pull request.
- [ ] Merge with a merge commit.

## After the merge
- [ ] Annotated tag on the merge commit: `git tag -a vX.Y.Z -m "Release vX.Y.Z"` and push the tag.
- [ ] GitHub release created from the tag with the release notes.
- [ ] Board: the sprint's tickets closed; the next sprint's iteration started.
- [ ] `dev` continues from the merge; nothing is committed to `main` directly.

## What a release is not
A release does not mean the software is safe to use in a vehicle by anyone other than the author under the operating rules of the safety statement. It means the checklist above was completed and the evidence is in the repository.
