"""Lightweight fail-closed tests for the native P0 receipt drivers (Windows)."""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / 'build/work/scripts'

# Each fixture program is a real child process. The CMake stub below skips only the
# configure/build steps; driver discovery, output capture, exit checks and hash
# comparisons operate through the same entry points used for real builds.
FIXTURE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    int million = 0, worker8 = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "1000000") == 0) million = 1;
        if (strcmp(argv[i-1], "--threads") == 0 && strcmp(argv[i], "8") == 0) worker8 = 1;
    }
    const char *mode = getenv("P0_FIXTURE_MODE");
    if (strstr(argv[0], "test_protocol")) {
        puts("fixture protocol test");
        return strcmp(mode, "test-fails") == 0 ? 3 : 0;
    }
    if (strcmp(mode, "silent") == 0) return 0;
    puts("fixture reference run");
    puts(strcmp(mode, "unlocked") ? "arena: 4 MB, locked=1, large_pages=0" : "arena: 4 MB, locked=0");
    puts(strcmp(mode, "wild-mode") ? "mode: LAB - seeded" : "mode: WILD - entropy");
    if (strcmp(mode, "missing-books") != 0)
        puts(strcmp(mode, "bad-books") ? "matter: 12 -> 12 (exact)   energy invariant: 34 -> 34 (exact)" :
             "matter: 12 -> 13 (exact)   energy invariant: 34 -> 34 (exact)");
    puts(strcmp(mode, "audit-error") ? "field_underflow 0 dropped_events 0 audit_failures 0" :
         "field_underflow 0 dropped_events 2 audit_failures 0");
    if (strcmp(mode, "no-hash") != 0 && strcmp(mode, "both-fail") != 0) {
        if ((strcmp(mode, "disagree") == 0 && strstr(argv[0], "clang-z")) || (worker8 && strcmp(mode, "thread-diff") == 0)) {
            puts("final hash 2222222222222222");
        } else if (strcmp(mode, "trailing") == 0) {
            puts("final hash 0123456789abcdeff");
        } else {
            puts("final hash 0123456789abcdef");
        }
        if (strcmp(mode, "duplicate") == 0) puts("final hash 0123456789abcdef");
    }
    return strcmp(mode, "exit-after-hash") == 0 || (strcmp(mode, "both-fail") == 0 || (million && strcmp(mode, "million-fail") == 0)) ? 7 : 0;
}
'''


def quote(path):
    return "'" + str(path).replace("'", "''") + "'"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tmp', type=Path, default=ROOT / 'build/tmp')
    args = parser.parse_args()
    args.tmp.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which('gcc.exe')
    pwsh = shutil.which('pwsh')
    if not compiler or not pwsh:
        raise SystemExit('requires gcc.exe and pwsh')
    failures = []
    with tempfile.TemporaryDirectory(dir=args.tmp, prefix='p0-drivers-') as temp:
        root = Path(temp)
        env = dict(os.environ, TMP=temp, TEMP=temp)
        scripts = root / 'build/work/scripts'
        scripts.mkdir(parents=True)
        for name in ['run_tests.ps1', 'clang_check.ps1', 'm_check.ps1']:
            shutil.copyfile(SCRIPTS / name, scripts / name)
        (root / 'tools').mkdir()
        (root / 'tools/lint_core.py').write_bytes(
            b'import os, sys\nprint("lint_core: 72 files, 0 errors, 3 warnings")\n'
            b'sys.exit(1 if os.environ["P0_FIXTURE_MODE"] == "lint-fail" else 0)\n')
        (root / 'fixture.c').write_bytes(FIXTURE.encode('ascii'))
        binary = root / 'fixture.exe'
        subprocess.run([compiler, str(root / 'fixture.c'), '-o', str(binary)],
                       env=env, check=True, cwd=root)
        for name in ['clang-z', 'z']:
            build = root / 'build' / name
            build.mkdir()
            (build / 'CMakeCache.txt').write_bytes(b'CMAKE_C_COMPILER:STRING=fixture\n')
            for exe in ['test_protocol.exe', 'az_run.exe', 'az_run_proto.exe']:
                shutil.copyfile(binary, build / exe)
        empty = root / 'empty'
        empty.mkdir()
        cases = [
            ('tests-ok', 'run_tests', 'ok', True),
            ('tests-empty', 'run_tests', 'empty', False),
            ('tests-nonzero', 'run_tests', 'test-fails', False),
            ('hash-ok', 'clang_check', 'ok', True),
            ('hash-missing', 'clang_check', 'no-hash', False),
            ('hash-silent-retry', 'clang_check', 'silent', False),
            ('hash-both-fail', 'clang_check', 'both-fail', False),
            ('hash-nonzero-after-hash', 'clang_check', 'exit-after-hash', False),
            ('hash-duplicate', 'clang_check', 'duplicate', False),
            ('hash-noncanonical', 'clang_check', 'trailing', False),
            ('hash-disagree', 'clang_check', 'disagree', False),
        ]
        for mode in ('ok', 'skip-million', 'build-fails', 'test-fails', 'silent', 'exit-after-hash',
                     'duplicate', 'missing-books', 'bad-books', 'audit-error', 'unlocked', 'wild-mode',
                     'thread-diff', 'million-fail', 'lint-fail'):
            cases.append(('mcheck-' + mode, 'm_check', mode, mode in ('ok', 'skip-million')))
        for case, name, mode, expect_ok in cases:
            log_dir = root / case
            if mode == 'silent':
                log_dir.mkdir()
                if name == 'm_check':
                    (log_dir / 'run100k_t1.log').write_bytes(
                        b'arena: 4 MB, locked=1\nmode: LAB - seed\n'
                        b'matter: 12 -> 12 (exact)   energy invariant: 34 -> 34 (exact)\n'
                        b'field_underflow 0 dropped_events 0 audit_failures 0\nfinal hash 0123456789abcdef\n')
                for world in ['ref100k', 'proto20k', 'proto20k_cap']:
                    for toolchain in ['clang', 'gcc']:
                        (log_dir / (world + '_' + toolchain + '.log')).write_bytes(
                            b'final hash 0123456789abcdef\n')
            script = scripts / (name + '.ps1')
            cmd = ('function cmake { "fixture configure/build"; $global:LASTEXITCODE = ' +
                   ('1' if mode == 'build-fails' else '0') + ' }; ')
            cmd += '& ([scriptblock]::Create([IO.File]::ReadAllText(' + quote(script) + '))) '
            if name == 'm_check':
                cmd += '-Root ' + quote(root) + ' -Out ' + quote(log_dir)
                if mode == 'skip-million':
                    cmd += ' -SkipMillion'
            elif name == 'clang_check':
                cmd += '-SkipMillion -Root ' + quote(root) + ' -Out ' + quote(log_dir)
            else:
                build = empty if mode == 'empty' else root / 'build/clang-z'
                cmd += '-BuildDir ' + quote(build) + ' -LogDir ' + quote(log_dir)
            proc = subprocess.run([pwsh, '-NoLogo', '-NoProfile', '-Command', cmd], cwd=root,
                                  env=dict(env, P0_FIXTURE_MODE=mode), capture_output=True,
                                  timeout=60)
            observed = proc.returncode == 0
            if observed and expect_ok:
                text = proc.stdout.decode('utf-8', errors='replace')
                expected_line = 'test_protocol: exit 0' if name=='run_tests' else 'agree on 3 worlds'
                if name == 'm_check':
                    expected_line = 'run100k_t24e: exit 0; final hash 0123456789abcdef'
                    if ('run1m: exit 0' in text) != (mode != 'skip-million') or 'lint: exit 0;' not in text:
                        failures.append(case + ' missing full/short or lint receipt')
                if expected_line not in text:
                    failures.append(case + ' missing positive receipt')
            if observed != expect_ok:
                failures.append(case + ' unexpected exit ' + str(proc.returncode))
                print((proc.stdout + proc.stderr).decode('utf-8', errors='replace')[-400:])
            print(case, 'exit', proc.returncode, 'expected success', expect_ok)
    print(len(cases), 'cases,', len(failures), 'failures')
    if failures:
        print('\n'.join(failures))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

