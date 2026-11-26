# Asmodensius-Native v1.1.0 task runner (one-word commands; Build Standard A31 & B01)
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
  $env:PATH = "$env:USERPROFILE\.elan\bin;$env:PATH"; Set-Location -LiteralPath 'lean'; lake build; lake build Challenge; lake env lean VerifyAxioms.lean; lake env leanchecker --fresh Solution; lake env leanchecker-paranoid --fresh Solution

paper:
  python -I scripts/build_paper.py; python -I scripts/check_citations.py --live

axioms:
  python -I scripts/check_readme_axioms.py

text:
  python -I build/work/scripts/check_text.py .

audit:
  pwsh -NoProfile -File scripts/Audit-Repo.ps1

checksums:
  python -I examples/parity_smoke.py --checksums

reproduce: lean paper axioms text audit
  @Write-Output "Clean-clone reproducible verification (Lean + Paper + Citations + Axioms + Text + Audit) complete."
