# AGENTS.md: Asmodensius-Native (Codex CLI reads this file at every start)

Root: Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native  (PowerShell: $R = '<root>' in single quotes)
Read first: docs\handoff\next-agent\00-MASTER-PROMPT.md. The current work package is the last "Next:"
line of build\work\PROGRESS.md; if that line points to 00-PROMPT.md, start at WP-P0-01 (master 9.1).
After any context compaction: re-read master section 3 and the current work package before writing.

## Laws (a breach is an S0 defect: stop, fix it with a failing test first, record it)
1. Native only: C23, NASM, PTX in the simulation; no VM, JIT, interpreter, WSL, Docker or QEMU.
2. Cache-first, one page-locked arena; no pagefile, virtual or NVMe RAM, compression, GPU oversubscription.
3. No LLM anything in the engine: no tokenizer, prompt, API, MCP, pretrained weights or dataset.
4. No theatre: every mechanism has a lesion effect, every trait is read; no steering or fitness function.
5. Exact books: integer matter and energy equal after every tick; field_underflow 0, dropped_events 0.
6. Wild by default (fresh OS entropy every tick); every evaluation run passes --lab; the default stays wild.
7. Nothing fixed: every number a gene or world parameter; refusals recorded, never a crash or silent clamp.
8. Indicator only: never claim consciousness, sentience, life, AGI or quantum effects; no aggregate score.
9. Honest evidence: "ran" only with the command and its output tail; unknown is fine, invented is not.
10. Windows only, from Z:; no boot image, USB boot, OS-less code or VM.
11. Z: only: never subst, net use, persistent PSDrive, junction or symlink; write only under the root.
12. Every word of the owner counts: new words go to docs\handoff\PROMPTS.md and OWNER-WORDS.md first.
13. The contract (core\az_abi.h, az_types.h, the .def files) changes only between pieces of work, recorded.
14. Text rule: ASCII, LF, no tabs, a final newline in every file you write (check_text.py).
15. Privacy: no API key in any file; web queries generic, never the owner's name, project or paths.
--allow-pageable and --no-lock are diagnostics only, never in a reported, benchmark or suite run.

## Needs the owner's yes (default: not done)
Installs or downloads; git commit or push; privilege or Windows setting changes; deleting anything you
did not create; spending; public novelty claims; build\work\api-run\Start-ApiRun.ps1.

## Machine and shell rules
- Never start servers, browsers or GUI windows; the owner opens the demo; every test is headless.
- Never run a tool with a guessed flag: read its usage first. az_lesion always gets --seeds (1-24).
- No tool call blocked over 4 minutes: long jobs in the background, logs on Z: (master 3.5).
- Quote every path; cmdlets get -LiteralPath; no > into the tree: pipe to Set-Content -LiteralPath.
- Start-Process -WorkingDirectory and -RedirectStandardOutput fail on the brackets: use & or .NET Process.
- Never pwsh -File a bracketed path: & ([scriptblock]::Create([IO.File]::ReadAllText("<file>.ps1"))).
- Python only for tooling, as python -I; glob needs glob.escape(folder); prefer pathlib and os.listdir.
- CMake globs go through ${AZ_GLOB_ROOT}; configure new build folders; never use N:-era build folders.
- Re-configure after adding core\az_<module>.c. Git Bash: single-quote the path. No backtick escapes.
- Set TMP and TEMP to <root>\build\tmp for every build or test process (process scope only).
- Old N:, T:, V:, W:, S: paths in documents: read them through master section 3.4; never map them.

## Session start (before any write)
1. subst prints nothing; Get-PSDrive shows no N:, S:, T:, V: or W:.
2. Other agents and files changed in the last 30 minutes (master 9.1 step 2); a live writer = read-only.
3. Read or take build\work\WRITER.lock (master 5.5).
4. Read the last Next: line and three entries of build\work\PROGRESS.md; open questions in DECISIONS.md.
5. Record the result of any background job of the last session.
6. Z: free space; below 20 GB delete only your own build outputs, then ask (master 5.1).

## Session end
1. Finish the step or record it PARTIAL; never leave build\z red.
2. A PROGRESS.md entry whose Next: line names the next work package.
3. python -I <root>\build\work\scripts\check_text.py <root> --since <minutes> --work exits 0 (PROMPTS.md/.xml exempt; master 5.1).
4. Remove build\work\WRITER.lock (unless your background job still writes; name it in the lock).
5. subst prints nothing.
6. A session report of at most 15 lines (master 7.1).
