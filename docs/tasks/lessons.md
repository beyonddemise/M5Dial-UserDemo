# Lessons

## 2026-10-30 — Don't assume there is no hardware; check first

- **Mistake:** the design said "the builder has no hardware", so all on-device verification was
  pushed onto the owner. An M5Dial was in fact plugged in via USB-C.
- **Rule:** for embedded/IoT work, before writing any verification plan, run a quick check
  (`ls /dev/cu.usb*`, `ioreg -p IOUSB`/`lsusb`) and ask if nothing shows up. Plan
  hardware-in-the-loop verification (flash + serial log) whenever a device is reachable,
  and leave only the truly human checks (visual, tactile, audio, external parts) to the owner.
- **Also:** flashing overwrites what's on the device, so back up the flash before the first write.

## 2026-10-30 — No compatibility shims that preserve legacy naming

- **Mistake:** the plan wrapped the M5Dial library in a facade that kept the original
  `hal->encoder.btn.read()` / `hal->tp` names to make porting easier.
- **Rule:** when porting to a target library, write code in that library's idioms and naming
  (`M5Dial.BtnA.wasPressed()`, `M5Dial.Touch.getDetail()`, `M5Canvas`). Readers won't know the
  legacy codebase; a shim optimises for the porter, not the maintainer. Port semantics, not names.

## 2026-10-30 — Batch human-only checks into one late task

- **Rule:** keep automated verification (host tests, simulation, flash + serial) inside each
  task; collect everything that needs a person (visual, tactile, cards, jig, phone) into a
  single acceptance task near the end, so execution isn't blocked waiting on the owner.

## 2026-10-30 — Verify the target library actually works before making it the contract

- **Mistake:** the design made `M5Dial.Encoder.read()` the encoder contract without checking it.
  M5Dial 1.0.3's `Encoder.h` has no interrupt pins above 39, so on GPIO40/41 it silently polls
  (m5stack/M5Dial#8). CodeRabbit caught it after the PR was merged.
- **Rule:** before a design depends on a third-party API for timing-critical I/O, read its source
  for the exact pins/board and search its open issues. Address review comments before merging.

## 2026-10-30 — Never chain review replies after a commit with `;`

- **Mistake:** `git add` failed (path under an ignored dir), the `&&` chain stopped before commit/push,
  but the reply commands ran on separate lines and posted "Fixed in :" with an empty sha.
- **Rule:** gate thread replies on a verified pushed sha (`[ -n "$sha" ] && git branch -r --contains "$sha"`)
  in the same `&&` chain; for tracked files in ignored dirs use `git add -u <path>`.

## 2026-11-03 — A checksum proves a copy, not a healthy device; don't diagnose from topology guesses

- **Mistake:** after a factory restore whose readback matched the backup's sha256, I called the
  firmware "verified good" and blamed power. I also took `05ac:800b` (the Mac's built-in USB2
  hub) for an external hub, although the board was in a Mac port directly. The owner corrected
  both. The backup later proved identical to an independent out-of-box dump, and M5Stack's
  UIFlow2 image failed too, so the unit is hardware-damaged (issue #3).
- **Rule:** a matching hash only proves the restore reproduced the image that was read. Say
  "restored bit-for-bit what was read on <date>", never "known good", unless the image comes
  from a trusted source (the vendor's binary, or a working unit). Before blaming cables, hubs
  or power, ask the owner about the physical setup or check it; don't infer it from USB IDs.

## 2026-11-03 — Stage files by name; directory-only ignore patterns miss symlinks

- **Mistake:** on the #15 branch I ran `git add sketch`. That worktree links `.arduino`, `backup`
  and the tool folders to a shared copy, and `sketch/.gitignore` used `backup/`-style patterns,
  which match directories only. Git records a symlink as a file, so five links to absolute paths
  on this Mac went into PR #42 and main.
- **Rule:** stage with explicit paths (`git add <file>...`) and check `git status --short` (or
  `git diff --cached --stat`) before every commit. Ignore patterns for folders that may be
  symlinks have no trailing slash (`/backup`, not `backup/`). `tools/tests/test_gitignore.py`
  guards this.

## 2026-11-07 — Local working folders stay out of the tree

- **Mistake:** the agent worktrees under `.superpowers/` (gitlinks, mode 160000) and its scratch
  builds were committed to main without a `.gitmodules`. Nothing noticed until a CI change set
  `persist-credentials: false`: `actions/checkout` then ran `git submodule foreach`, which
  fails on an undeclared gitlink, and every job failed at checkout (PR #45).
- **Rule:** keep worktrees and scratch output in an ignored folder (`/.superpowers` in the root
  `.gitignore`), and before committing check `git diff --cached --stat` for paths outside the
  change. `tools/tests/test_gitignore.py` fails if the tree tracks any gitlink.

## 2026-11-08 — A PR can be merged while work on it continues

- **Mistake:** the owner merged #49 and #50 while the agent was still pushing to them. A fix
  pushed 25 s after #49's merge re-created the deleted branch and never reached main, while its
  CodeRabbit threads were already marked "Fixed in <sha>". A commit held back for #50 (to avoid
  restarting its scenario run) was stranded the same way.
- **Rule:** before every push to a PR branch, check `gh pr view <n> --json state` is `OPEN`. If
  it was merged, put the commits on a new branch from `origin/main` and open a follow-up PR that
  names the merged one; never push to the deleted branch name. After the push, check the state
  again: if it merged in between, confirm the merged main contains the pushed commit
  (`git fetch origin main && git merge-base --is-ancestor <sha> origin/main`; without the fetch
  `origin/main` can predate the merge) before resolving any review thread as
  fixed, and move what is missing to a follow-up PR.
- **Also:** a stale build output can be newer than its source (a tracked `dist/` gets a fresh
  checkout time), so `make` skips the rebuild. That hid a Wokwi schema change until #48
  untracked `dist/`; `make zip` now always rebuilds.
