# Frequently Asked Questions (`docs/faq.md`)

## Does `Asmodensius-Native` claim consciousness, sentience, or biological life?

No. Project Law 8 strictly forbids claiming consciousness, sentience, life, AGI, or quantum speedup. All biological terminology labels deterministic integer data structures. The `butlin14` catalog is an engineering indicator checklist only, and there is no aggregate score.

## Does `Asmodensius-Native` use an LLM or pretrained neural network?

No. Project Law 3 forbids any LLM, tokenizer, prompt, API, MCP server, pretrained weights, or external dataset inside the simulation engine. Every organism's small sparse `int8` recurrent network develops from its own inherited diploid genome and updates only through local integer rules.

## Does `core/tk_qsv.c` or `core/tk_qgt.c` use quantum hardware or claim quantum speedup?

No. Both files are classical fixed-point linear-algebra routines running on ordinary x86-64 integer units. They produce zero quantum randomness and zero quantum speedup (`THIRD_PARTY_NOTICES.md`).

## Why is 24-core CPU faster than the RTX 5070 Ti GPU at 1,000,000 creatures?

Because the creature store currently lives in host RAM as two-half compacting variable-size blocks (`TR-14`). At 1M live creatures (`9,260.6 MB` locked), synchronizing dirty variable-size blocks across PCIe makes `--gpu 1` (`1,122.495 ms/tick`, 0 fallbacks, bit-exact hash `31e5d00c106ce679`) slower than 24C AVX-VNNI+PF CPU (`757.988 ms/tick`). Moving the entire world state into VRAM is tracked as open capability `TR-05`.

## Why does `--large-pages` report `large_pages=0` on Windows?

Windows `MEM_LARGE_PAGES` requires the OS privilege `SeLockMemoryPrivilege` ("Lock pages in memory"), which standard non-elevated user tokens do not hold. `Asmodensius-Native` falls back cleanly to 4 KiB `VirtualLock` (`locked=1, large_pages=0`) rather than crashing or using pageable memory (`TR-03`).

## Does `Asmodensius-Native` solve any problem in the Unsolved Problems Mega-Atlas?

No. **`0 / 1,900`** overarching Mega-Atlas open problems are solved or retired. See [MEGA_ATLAS_CROSSCHECK.md](MEGA_ATLAS_CROSSCHECK.md).

## Why does `check_text.py` reject non-ASCII characters and CRLF?

Project Law 14 asks for pure 7-bit ASCII, LF (`\n`) line endings, zero tabs (except `.tsv` data tables and POSIX TSV parsers), and a trailing newline in every edited text file. Starting in `v1.2.0`, the tracked checker `tools/check_text.py` (with a 6/6 planted-fault self-test) runs in CI and enforces 0 violations across all active tracked source, Lean, paper, script, workflow, and active documentation files while transparently counting and reporting verbatim owner prompts (`docs/handoff/PROMPTS.*`, 2 exempt) and historical archival dumps (`docs/requirements/`, `docs/research/`, `tests/upstream/`). See `ERRATA.md` and `LIMITATIONS.md`.

## Can I verify the Lean 4.35.0 proofs without a Windows machine or NVIDIA GPU?

Yes. The entire 30-theorem Lean 4.35.0 suite (`lean/`), the AMS-LaTeX white paper (`scripts/build_paper.py`), the live citation auditor (`scripts/check_citations.py --live`), and the Build Standard audit (`scripts/audit-repo.sh`) run identically on Linux, including all 6 Lean kernels under `bubblewrap`.
