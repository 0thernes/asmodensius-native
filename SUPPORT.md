# Support (`SUPPORT.md`)

## Where to Get Help

- **Documentation first:** Read [README.md](README.md), [docs/faq.md](docs/faq.md), [docs/configuration.md](docs/configuration.md), [docs/architecture.md](docs/architecture.md), and [REPLICATION.md](REPLICATION.md).
- **Bug reports & questions:** Open a GitHub Issue using the structured forms in `.github/ISSUE_TEMPLATE/` (`bug.yml`, `feature.yml`, `question.yml`).
- **Direct contact:** `0_0@0thernes.art` (Alexander Stephen Donahue, Independent Researcher, Spokane, WA, USA).

## Information to Include in Support Requests

1. Operating system, CPU topology, GPU model/driver (if using `--gpu 1`), and `git rev-parse HEAD`.
2. Exact command executed (including `--lab --seed` or wild mode).
3. Verbatim stdout/stderr tail and exit code (Project Law 9: honest evidence).
4. Whether `ctest --test-dir build/z --output-on-failure` and `python -I build/work/scripts/check_text.py .` pass cleanly.
