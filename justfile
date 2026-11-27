# Asmodensius-Native v1.2.0 task runner (one-word commands; Build Standard A31 & B01)
set shell := ["pwsh", "-NoProfile", "-Command"]

default:
  @just --list

build:
  $r = (Get-Location).Path; $env:TMP = Join-Path $r 'build\tmp'; $env:TEMP = $env:TMP; [void][IO.Directory]::CreateDirectory($env:TMP); cmake -S $r -B (Join-Path $r 'build\z') -G Ninja; cmake --build (Join-Path $r 'build\z')

test:
  $r = (Get-Location).Path; $env:TMP = Join-Path $r 'build\tmp'; $env:TEMP = $env:TMP; ctest --test-dir (Join-Path $r 'build\z') --output-on-failure

gpu:
  $r = (Get-Location).Path; $env:TMP = Join-Path $r 'build\tmp'; $env:TEMP = $env:TMP; & (Join-Path $r 'build\z\test_gpu.exe')

lean:
  $env:PATH = "$env:USERPROFILE\.elan\bin;$env:PATH"; Set-Location -LiteralPath 'lean'; lake build; lake build Challenge; lake env lean VerifyAxioms.lean; lake env leanchecker --fresh Solution; lake env leanchecker-paranoid --fresh Solution; Set-Location -LiteralPath '..'; python -I scripts/test_lean_negative.py

paper:
  python -I scripts/build_paper.py; python -I scripts/check_citations.py --live

axioms:
  python -I scripts/check_readme_axioms.py; python -I scripts/check_blueprint.py

text:
  python -I tools/check_text.py --self-test; python -I tools/check_text.py

timestamps:
  python -I scripts/stamp_timestamps.py --verify

audit:
  pwsh -NoProfile -File scripts/Audit-Repo.ps1

checksums:
  python -I examples/parity_smoke.py --checksums

reproduce: lean paper axioms text timestamps audit
  @Write-Output "Clean-clone reproducible verification (Lean + Negative + Paper + Citations + Axioms + Blueprint + Text + Timestamps + Audit) complete."
