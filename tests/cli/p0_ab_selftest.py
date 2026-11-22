"""Run the real PowerShell A/B command against native positive and negative controls."""
import argparse
import csv
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / 'build/work/scripts/ab_time.ps1'
C = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    const char *m = getenv("AB_CONTROL");
    int b = strstr(argv[0], "bench-b") != NULL;
    int grid = 0, lab = 0, worker = 0;
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i],"--lab")) lab = 1;
        if (!strcmp(argv[i-1],"--grid") && !strcmp(argv[i],"3,3,3")) grid = 1;
        if (!strcmp(argv[i-1],"--threads") && !strcmp(argv[i],"1")) worker = 1;
    }
    if (!grid || !lab || !worker) return 42;
    printf("arena: 7.5 MB, locked=%d, large_pages=0\n", strcmp(m,"unlocked")!=0);
    if (!strcmp(m,"wild")) puts("mode: WILD - fresh");
    else printf("mode: LAB - the seeded, repeatable test bench (seed %d)\n", b && !strcmp(m,"seed-diff")?43:42);
    printf("world: grid 8x8x8 (512 cells), cap 256, initial %d, species 100; grid bits 3,3,3 given\n",b && !strcmp(m,"world-diff")?31:32);
    if (strcmp(m,"missing-timing"))
        printf("ticks %d in 0.001 s = 2000.0 ticks/s; %s ms/tick; live 32 (slots 32); 4429.7 ns per live entity per tick\n",
               b && !strcmp(m,"ticks-diff")?3:2,!strcmp(m,"nan")?"NaN":"0.150");
    printf("stages (ms/tick): reorder=0.010 grid=0.014 %s\n",!strcmp(m,"missing-stage")?"act=0.025":
           !strcmp(m,"inf-stage")?"perceive=Infinity":"perceive=0.025");
    if (strcmp(m,"missing-hash")) printf("final hash %s\n", b && !strcmp(m,"hash-diff")?"2222222222222222":"5b3052860fffddd2");
    if (!strcmp(m,"duplicate")) puts("final hash 5b3052860fffddd2");
    return !strcmp(m,"bad-exit") ? 3 : 0;
}
'''
def q(value):
    return "'" + str(value).replace("'", "''") + "'"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tmp', type=Path, default=ROOT/'build/tmp')
    args = parser.parse_args()
    args.tmp.mkdir(parents=True, exist_ok=True)
    failures = []
    modes = ['raw', 'array', 'missing-timing', 'nan', 'missing-stage', 'inf-stage',
             'missing-hash', 'hash-diff', 'duplicate', 'bad-exit', 'seed-diff',
             'world-diff', 'ticks-diff', 'wild', 'unlocked', 'no-rounds', 'pinned-divergence', 'wrong-a-pin', 'wrong-b-pin', 'one-pin', 'pinned-world-diff']
    with tempfile.TemporaryDirectory(dir=args.tmp, prefix='ab-controls-') as temp:
        folder = Path(temp)
        env = dict(os.environ, TMP=temp, TEMP=temp)
        c = folder/'child.c'
        a, b = folder/'bench-a.exe', folder/'bench-b.exe'
        c.write_bytes(C.encode('ascii'))
        subprocess.run(['gcc.exe', str(c), '-o', str(a)], check=True, cwd=folder, env=env)
        shutil.copyfile(a, b)
        for mode in modes:
            dest = folder/(mode+'.csv')
            ok = mode in ['raw', 'array','pinned-divergence']
            sentinel = b'previous,complete\n'
            if not ok:
                dest.write_bytes(sentinel)
            # Only the 2 s CPU sampler is simulated. The timing driver, parser,
            # actual per-arm program exit codes and output files are exercised.
            command = "function Get-Counter { [pscustomobject]@{ CounterSamples = @([pscustomobject]@{ CookedValue = 4.0 }) } }; "
            command += '& ([scriptblock]::Create([IO.File]::ReadAllText(' + q(DRIVER) + '))) -A '+q(a)+' -B '+q(b)+' -Csv '+q(dest)
            command += ' -Rounds '+('0' if mode=='no-rounds' else '2')
            if mode in ('pinned-divergence','wrong-a-pin','wrong-b-pin','one-pin','pinned-world-diff'):
                command += " -AHash " + ('4444444444444444' if mode=='wrong-a-pin' else '5b3052860fffddd2')
                if mode != 'one-pin':
                    command += " -BHash " + ('1111111111111111' if mode=='wrong-b-pin' else
                                             '5b3052860fffddd2' if mode=='pinned-world-diff' else '2222222222222222')
            if mode == 'raw':
                command += ' -- --lab --n 32 --grid 3,3,3 --threads 1 --ticks 2'
            else:
                command += " -RunArgs @('--lab','--n','32','--grid','3,3,3','--threads','1','--ticks','2')"
            result = subprocess.run(['pwsh','-NoLogo','-NoProfile','-NonInteractive','-Command',command],
                                    cwd=folder,env=dict(env,AB_CONTROL=('world-diff' if mode=='pinned-world-diff' else 'hash-diff' if mode in ('pinned-divergence','wrong-a-pin','wrong-b-pin','one-pin') else mode)),capture_output=True,timeout=30)
            passed = (result.returncode == 0) == ok
            if ok and passed:
                with dest.open(encoding='utf-8-sig',newline='') as f:
                    rows=list(csv.DictReader(f))
                passed = ([row['which'] for row in rows]==['A','B','A','B'] and
                          all(row['hash']==('2222222222222222' if mode=='pinned-divergence' and row['which']=='B' else '5b3052860fffddd2') and float(row['tick_ms'])==0.15 and
                              float(row['stage_ms'])==0.025 for row in rows))
            if not ok:
                passed = passed and dest.read_bytes()==sentinel
            print(mode,'exit',result.returncode,'PASS' if passed else 'FAIL',flush=True)
            if not passed:
                failures.append(mode)
    print(len(modes),'cases,',len(failures),'failures')
    return bool(failures)

if __name__ == '__main__':
    raise SystemExit(main())
