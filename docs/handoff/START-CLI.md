# Start the Asmodensius-Native build in the Claude Code CLI

> **Update, 2026-11-18.** The 200 USD API credit is used up: the API answered "Credit balance is too low" at 12:04 UTC, and a one-cent test at 14:30 UTC was refused. The sheet below is history. The next agent (the owner plans OpenAI GPT Astra 6.2) starts from `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\docs\handoff\next-agent\00-PROMPT.md`. A new Claude run would need fresh credit and a rewritten item 3 in `build\work\api-run\kickoff.md`.

> **Billing correction, 2026-11-17.** The 200 USD is the Ultra plan's monthly API credit. It can't pay for interactive Claude Code with any login method, so step 2 below won't make it work. It pays for the Agent SDK (`claude -p`). To run the build on it, use `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\api-run\Start-ApiRun.ps1`, as its README describes. Started this way, the CLI runs on the Ultra plan's usage limits.

The full instructions for the agent are in `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\docs\handoff\CONTINUE-BUILD.md`
(written and checked 2026-11-17). This sheet is only how to start it.

1. In PowerShell Preview:

       Set-Location -LiteralPath 'Z:\[Vibe Coded (AI)]\CLAUDECODE'

   (Z: only since 2026-11-18: no `subst`, no drive letters. The project root is
   `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`; older copies of this sheet wrote it
   as the letter N:.)

   Start in the CLAUDECODE folder: your memory notes load from there.

2. Billing, once. If the 200 USD is prepaid Anthropic Console (API) credit:
   `claude auth login --console`. Check with `claude auth status --text`.
   Turn auto-reload off for that credit, so the balance itself is a hard cap.
   (`claude auth login --claudeai` switches back to your plan later.)

3. Launch:

       claude -n asmodensius-full-build --model 'claude-fable-5-2[1m]' --effort max --permission-mode auto --add-dir 'Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native' --autocompact 400k --strict-mcp-config

   - Keep `--effort`: settings.json sets effortLevel to "low". `--effort high`
     stretches the credit; Anthropic suggests high as the starting level for
     Fable 5.2.
   - `--strict-mcp-config` is optional: it skips your MCP servers (Context7
     too), so every agent starts with less context.
   - If `--model` is rejected, leave it out: settings.json already selects
     that model.

4. Type this, with your real balance in place of 200:

       The project root is Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native. Read docs\handoff\CONTINUE-BUILD.md there in full and follow it. It points you to my prompt list (docs\handoff\PROMPTS.md) and your reports (docs\handoff\STATUS_AND_PLAN.md, build\work\logs\review_result.json, build\work\fix-majors). If build\work\PROGRESS.md exists, resume from its next step. You may run workflows as section 5 of that file allows: one package per workflow, at most 5 agents, every agent at effort high. My credit left is 200 USD. Stop cleanly at the stop line the file sets.

   Ultracode is recommended off for this run: that line already allows small
   workflows, while ultracode turns every prompt into a workflow. If you keep
   it on, the file still limits each workflow to one package of at most 5
   agents.

5. If the window closes: do step 1, then run the step 3 line with
   `--resume asmodensius-full-build` in place of `-n asmodensius-full-build`,
   and type:

       In Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native, resume from build\work\PROGRESS.md as docs\handoff\CONTINUE-BUILD.md section 5 says.

While it runs: progress is in `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\PROGRESS.md`. The spend
estimate is
`python -I "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\scripts\spend.py" --find CONTINUE-BUILD.md --since <start time at the top of PROGRESS.md>`.
It is a lower bound; your real balance is the check. Keep Codex off the project root while
the run lasts.
