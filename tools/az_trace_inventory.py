"""Verify the released A-Life source inventory and its historical relationships.

Run from any working directory: python -I tools/az_trace_inventory.py
By default the project uses the master checklist in its parent directory.
The --master-csv option can select another copy of the same source.
"""
import argparse
from collections import Counter, defaultdict
import csv
import json
from pathlib import Path
import re
import sys


COLUMNS = 'catalog item_id harvest_record source_ref canonical_id relation disposition reason_arg test_id prereg_file prereg_sha256 seed_block verdict d_z_min_arm ci_lo ci_hi p_iut p_holm n_pairs run_id raw_dir honesty historical_label provenance notes'.split()

DECISIONS = 'catalog item_id canonical_id relation disposition reason_arg test_id honesty notes'.split()

RELATIONS = set('SELF DUPLICATE-OF MIRROR-OF SUPERSEDED-BY ROLLUP-OF PART-OF'.split())

REASONS = set('NOT-BUILT N/A-ENGINE LAW-CONFLICT IDEA-BANK OUTSIDE-ENGINE DUPLICATE-OF SUPERSEDED-BY NEEDS-OWNER'.split())

HIST = dict(antilarp13=13,antilarp13_v40impl=13,antilarp_mutants5=5,apex25=25,bench_budgets=20,bench_foundations=99,bench_freak100=100,bench_life100=500,bench_novel100=100,benchmarks100=109,butlin14=14,checklist500=500,consciousness10=10,cqm25=25,cqm25_framework=25,cqm25_rules=19,cqm_scrutiny25=25,cube1000=1000,frontier_upgrade_map=272,gates9=9,laws_and_errata=333,life100_areas=100,life100_v40_500=500,nested_precision=134,passes100=100,passes100_suite=100,passes100_v40=100,thousand_reports=150,thousand_reports_index=900,wishlist=512)

EXTRACTS = {'framework825_'+k:n for k,n in dict(citations15=15,errata95=95,freak100=100,hw24=24,milestones100=100,mirrors5=5,novel100=100,soft40=40,wse12=12).items()}

EXPECTED = dict(HIST,**EXTRACTS,common_laws_native=2)

SUITES = dict(L4_FREAK100='framework825_freak100',L6_NOVEL100='framework825_novel100',L7_WSE12='framework825_wse12',L8_SOFT40='framework825_soft40',L9_HW24='framework825_hw24')

def check(ok,msg):
    if not ok: raise ValueError(msg)

def portable_text(value):
    # A single HTML numeric reference round-trips every non-ASCII source character.
    # Quote a literal reference's ampersand first, so one decoding pass is lossless.
    value=re.sub(r'&(?=#x[0-9a-fA-F]+;)',r'&#x26;',value)
    return ''.join(ch if ch=='\n' or 32<=ord(ch)<=126 else f'&#x{ord(ch):x};' for ch in value)

def source_text(value):return re.sub(r'&#x([0-9a-fA-F]+);',lambda m:chr(int(m[1],16)),value)

def read_csv(path,header=None):
    with path.open(encoding='utf-8-sig',newline='') as stream:
        reader=csv.DictReader(stream,strict=True)
        check(reader.fieldnames and len(reader.fieldnames)==len(set(reader.fieldnames)),f'{path}: duplicate or absent header')
        if header:check(reader.fieldnames==header,f'{path}: expected columns {header}')
        result=list(reader)
        check(all(None not in row and None not in row.values() for row in result),f'{path}: wrong field count')
        if header in (COLUMNS,DECISIONS):return [{k:source_text(v) for k,v in row.items()} for row in result]
        return result

def unique(rows,key,label):
    out={}
    for row in rows:
        k=key(row);check(k not in out,f'{label}: duplicate {k}');out[k]=row
    return out

def key(row):return row['catalog']+'/'+row['item_id']

def line(fields):return '; '.join(str(x) for x in fields if x)

def sources(root):
    out={}
    available={path.stem for path in (root/'docs/requirements').glob('*.json')}-{'checklists_index'}
    check(available==set(HIST)|set(EXTRACTS),'catalog inventory differs from protocol: '+str(sorted(available^(set(HIST)|set(EXTRACTS)))))
    for cat,n in {**HIST,**EXTRACTS}.items():
        path=root/'docs/requirements'/(cat+'.json')
        obj=json.loads(path.read_bytes())
        check(len(obj['items'])==n,f'{cat}: source count differs from {n}')
        for i,item in enumerate(obj['items']):
            id_=item['id']; k=cat+'/'+id_
            check(id_ and k not in out,'duplicate source '+k)
            out[k]=(cat,id_,f'docs/requirements/{cat}.json#/items/{i}',item)
    text=(root/'docs/agents/COMMON.md').read_text(encoding='utf-8').splitlines()
    for n,topic in [(9,'Windows only, from Z:'),(10,'Below assembly, inside Windows.')]:
        idx=next(i for i,x in enumerate(text) if x.startswith(f'{n}. **'+topic))
        stop=idx+1
        while stop<len(text) and text[stop].startswith('   '):stop+=1
        id_='COMMON-LAW-'+str(n);ref='docs/agents/COMMON.md#L'+str(idx+1)
        out['common_laws_native/'+id_]=('common_laws_native',id_,ref,dict(text=' '.join(t.strip() for t in text[idx:stop]),criterion='',category='Native supplementary law',source_ref=ref))
    return out

def historic(item):
    labels=[];orig=[]
    if item.get('status_r21'):
        labels.append('R21 arm A | '+item['status_r21']);orig.append('v40 R21 seeds 1064-1111')
    for k,v in item.get('extra',{}).items():
        if k in ('status_pdf','r21_armA_status','r21_armA_d_t','r21_armA_detail','combo_a_result','creature_status_tsv','recorded_ok','recorded_detail','measured_v5_0_0','gate') and v not in ('',None):
            labels.append(k+'='+str(v));orig.append(k)
    cat=item.get('category','')
    if 'v40 status' in cat:labels.append(cat.split('v40 status',1)[1].strip());orig.append('Windows v40 LIFE-100')
    for v in item.get('criterion','').splitlines():
        if 'Recorded status wording:' in v:
            labels.append('Recorded status: '+v.split('Recorded status wording:',1)[1].strip());orig.append('historical pipeline record')
        if v.startswith(('Source status:','Source resolution label:','Source closure disposition:','Source register status:','Status:','Disposition:')):labels.append(v);orig.append('historical source field')
    return line(labels),line(dict.fromkeys(orig))

def form(source,decisions):
    rules=unique(decisions,key,'dispositions')
    check(set(source)==set(rules),f'disposition input keys differ: missing {sorted(set(source)-set(rules))[:4]}, extra {sorted(set(rules)-set(source))[:4]}')
    out=[]
    for k,(cat,id_,ptr,item) in source.items():
        rule=rules[k]; row=dict.fromkeys(COLUMNS,'')
        for name in DECISIONS:row[name]=rule[name]
        row.update(harvest_record=ptr,source_ref=item['source_ref'])
        row['historical_label'],origin=historic(item)
        row['provenance']=line([origin,item['source_ref']])
        row['notes']=line([row['notes'],'Source wording: '+item['text'],('Source criterion: '+item['criterion']) if item['criterion'] else '',('Alternative template criterion: '+item['template_criterion']) if item.get('template_criterion') else ''])
        if cat=='butlin14' and id_=='AE-1':
            row['historical_label']=line([row['historical_label'],'SC-PORT public c72a4ec9a0cb / bc6ad6a | AE1 d 0.73, t 5.48'])
            row['provenance']=line([row['provenance'],'public c72a4ec9a0cb; local Windows v40 export; protocol 07 section 5.7'])
            row['notes']+='\nSC-PORT local v40 Windows export | AE1 d 0.86, t 6.41. Each archive has 112 observations per arm; source recomputation gives 0.73267 and 0.85702. Cause unknown.'
        out.append(row)
    return out

def targets(row):
    return [x.strip() for x in row['canonical_id'].split(';') if x.strip()]

def csv_data(rows,columns=COLUMNS):
    import io
    stream=io.StringIO(newline='');writer=csv.DictWriter(stream,fieldnames=columns,lineterminator='\n');writer.writeheader();writer.writerows(({k:portable_text(str(v)) for k,v in row.items()} for row in rows))
    return stream.getvalue().encode('utf-8')

def esc(v):return portable_text(v).replace('\\','\\\\').replace('|','\\|').replace('\r','').replace('\n','<br>')

def markdown(rows):
    groups=defaultdict(list)
    for row in rows:groups[row['catalog']].append(row)
    text=['# Native trace matrix','','Each source item retains its own identifier, historical provenance and source criterion. The ASCII files use reversible numeric character references for Unicode source text. Empty native dispositions identify outstanding evidence. The supplementary COMMON laws are counted separately from the harvested catalogs.','','| Catalog | Source items | Dispositions |','|---|---:|---|']
    for cat,items in groups.items():
        totals=Counter(x['disposition'] or 'Pending' for x in items)
        text.append('| '+cat+' | '+str(len(items))+' | '+', '.join(x+' '+str(n) for x,n in sorted(totals.items()))+' |')
    for cat,items in groups.items():
        text+=['','## '+cat,'','| Source ID | Relation and target | Disposition | Native verdict | Historical label |','|---|---|---|---|---|']
        for row in items:
            text.append('| '+' | '.join(map(esc,[row['item_id'],row['relation']+' '+row['canonical_id'],row['disposition'] or 'Pending',row['verdict'],row['historical_label']]))+' |')
    return ('\n'.join(text)+'\n').encode('utf-8')

def source_joins(source, rows):
    """Recompute the code graph; the reviewed input decides the scope of each code match."""
    index = {key(row):row for row in rows}
    def items(cat):
        return {k:v[3] for k,v in source.items() if v[0] == cat}
    ledger = {k:set(v['extra']['target_codes']) for k,v in items('passes100').items()}
    suite = {k:set(v['extra']['codes']) for k,v in items('passes100_suite').items()}
    left = {k:[] for k in ledger}
    right = {k:[] for k in suite}
    for l, codes in ledger.items():
        for s, other in suite.items():
            if codes & other:
                left[l].append(s)
                right[s].append(l)
    for s,adj in right.items():
        row = index[s]
        links = [k for k in targets(row) if k in ledger]
        if row['relation']=='ROLLUP-OF':continue
        if links:
            check(set(links) <= set(adj),s+': ledger join has no common code')
            check(row['relation'] in ('DUPLICATE-OF','PART-OF') and row['notes'],s+': ledger join requires a reviewed comparison')
            if len(adj)==1:check(links==adj,s+': wrong unique code partner')
        if adj and not links:
            check((row['relation']=='SELF' and 'These targets are distinct' in row['notes']) or (row['relation']=='SUPERSEDED-BY' and 'G-BOOKS-10K' in row['test_id']),s+': unresolved candidate code join')
        if not suite[s] and row['relation']!='SELF':
            check(row['relation']=='SUPERSEDED-BY' and all(k not in ledger for k in targets(row)),s+': codeless positional join')
    hw=index['framework825_hw24/HW02']
    check(hw['disposition'] in ('','GATE') and hw['test_id']=='HW02','HW02 mixed native and web requirements need both native checks')
    areas=items('life100_areas')
    for k,v in areas.items():
        expected=['checklist500/'+v['id']+'-A'+str(i) for i in range(1,6)]
        check(index[k]['relation']=='ROLLUP-OF' and targets(index[k])==expected,k+': LIFE subcheck rollup')
    for catalog in ('life100_v40_500','bench_life100'):
        for k,item in items(catalog).items():
            row=index[k]
            if catalog=='bench_life100':
                match=re.fullmatch(r'LIFE-(\d{3})\.([1-5])',item['id'])
                check(match and int(match[1])==item['extra']['area_number'] and int(match[2])==item['extra']['attribute_index'],k+': inconsistent PDF source area')
            else:match=re.fullmatch(r'V40-LIFE-L(\d{3})-A[1-5]',item['id'])
            check(match,k+': malformed area identity')
            expected='life100_areas/LIFE-L'+match[1]
            check(row['relation']=='PART-OF' and row['disposition']=='SUPERSEDED-BY' and targets(row)==[expected] and row['reason_arg']==expected,k+': historical edition target disagrees with source area')
    for k in items('passes100_v40'):
        row=index[k]; expected='passes100/'+source[k][1]
        check(targets(row)==[expected] and row['relation']=='DUPLICATE-OF' and row['reason_arg']==expected,k+': historical ledger counterpart')
    aggregates={}
    for group in ('passes100','passes100_suite'):
        records=items(group)
        by_number=unique(records,lambda k:int(records[k]['extra']['pass']),group+' ordinal')
        check(set(by_number)==set(range(1,101)),group+': expected 100 chronological source passes')
        k=by_number[100]
        deps=[by_number[i] for i in range(1,100)]+['laws_and_errata/LAW-5']
        row=index[k]
        check(row['relation'] == 'ROLLUP-OF' and targets(row) == deps and not row['disposition'],
              k + ': source aggregate requires its 99 preceding items and the books gate')
    for cat in ('consciousness10','butlin14'):
        for k,item in items(cat).items():
            alias='benchmarks100/'+item['id'];row=index[alias]
            check(row['relation']=='DUPLICATE-OF' and targets(row)==[k] and row['reason_arg']==k,alias+': direct historical alias disagrees with instrument ID')
    for k,item in items('benchmarks100').items():
        if 'reference' not in item['category'].lower() or item['id'] in ('C16R','K1R'):continue
        name=item['extra']['replaces'];candidates=[g+'/'+name for g in ('butlin14','consciousness10') if g+'/'+name in source]
        if re.fullmatch(r'CQM\d{2}',name):candidates=targets(index['cqm25_framework/'+name])
        check(len(candidates)==1 and targets(index[k])==candidates and index[k]['relation']=='PART-OF' and index[k]['reason_arg']==candidates[0],k+': R variant target disagrees with named original')
    check(targets(index['consciousness10/ctm'])==['butlin14/GWT-3'] and index['consciousness10/ctm']['relation']=='PART-OF','CTM partial overlap')
    check(targets(index['consciousness10/butlin14'])==list(items('butlin14')),'Butlin theory summary membership')
    for pos,axis in enumerate(items('cqm25'),1):
        for cat in ('cqm25_framework','benchmarks100'):
            k=f'{cat}/CQM{pos:02d}'
            check(targets(index[k])==[axis] and index[k]['relation']=='DUPLICATE-OF',k+': positional law axis')
            if pos in (9,14,24,25):check('drift' in index[k]['notes'].lower(),k+': missing wording drift')
    for k in items('benchmarks100'):
        row=index[k]; iid=row['item_id']
        if k.endswith('/C16R'):
            check(any(t.startswith('passes100_suite/') and 'C16R' in suite[t] for t in targets(row)),k+': R variant code join')
        if k.endswith('/K1R') and not any('K1R' in x for x in suite.values()):
            check(row['disposition']=='NEEDS-OWNER',k+': unlinked R variant needs decision')
        if iid.startswith('Q-'):
            matched={j for j,v in source.items() if v[0] in ('butlin14','cqm25','consciousness10') and v[3].get('quality_id')==iid}
            check((matched and set(targets(row))==matched) or (not matched and row['disposition']=='NEEDS-OWNER'),k+': quality identifier mismatch')
    mirror_sources=list(items('framework825_mirrors5').items())
    instrument={'HW18':'laws_and_errata/LAW-5','F2':'consciousness10/iit4'}
    for mirror,rec in mirror_sources:
        first,second=rec['category'].split(': ',1)[1].split(' \u2194 ')
        anchor=instrument.get(second,'butlin14/'+second)
        peer='framework825_hw24/HW18' if second=='HW18' else targets(index['cqm25_framework/'+first])[0]
        for candidate in (mirror,peer):
            check(index[candidate]['relation']=='MIRROR-OF' and targets(index[candidate])==[anchor],candidate+': wrong native mirror anchor')
    ae=index['butlin14/AE-1']
    check('d 0.73' in ae['historical_label'] and 'd 0.86' in ae['notes'],'AE-1 historical archives not both identified')

def framework_crosswalk(master,source,trace):
    """Resolve all 825 master slots against harvested sources, independently of the Astra workbook."""
    lookup={key(row):row for row in trace}
    theories={v[3]['template_id']:k for k,v in source.items() if v[0]=='consciousness10'}
    groups=[('CQM05','HW18','laws_and_errata/LAW-5'),('CQM13','HOT-2','butlin14/HOT-2'),
            ('CQM19','RPT-1','butlin14/RPT-1'),('CQM20','GWT-3','butlin14/GWT-3'),
            ('CQM21','F2','consciousness10/iit4')]
    mirrors={id_:(f'MIR-{i:02d}',anchor) for i,(a,b,anchor) in enumerate(groups,1) for id_ in (a,b)}
    for i,(_,_,anchor) in enumerate(groups,1):
        k=f'framework825_mirrors5/MIR-{i:02d}'
        check(targets(lookup[k])==[anchor] and lookup[k]['relation']=='MIRROR-OF',k+': incorrect shared subtest')
    mapped=[]
    for rec in master:
        if rec['Record_Type']!='CAPABILITY_CHECK':continue
        id_=rec['ID']; suite=rec['Suite_Or_Stage']; mirror,anchor=mirrors.get(id_,('',''))
        if suite=='L1_BUTLIN14':key_='butlin14/'+id_
        elif suite=='L2_FRAMEWORKS10':key_=theories[id_]
        elif suite=='L3_CQM25':key_=targets(lookup['cqm25_framework/'+id_])[0]
        elif suite=='L5_LIFE500':key_='checklist500/'+id_
        else:key_=SUITES[suite]+'/'+id_
        check(key_ in source,'unmapped master slot '+id_)
        mapped.append(dict(suite=suite,check_id=id_,source_catalog_id=key_,harvest_record=source[key_][2],
                           framework_mirror=mirror,shared_subtest_anchor=anchor))
    check(len(unique(mapped,lambda x:x['check_id'],'master framework'))==825,'master requires 825 distinct slots')
    check(Counter(x['suite'].split('_')[0] for x in mapped)==dict(zip(('L1','L2','L3','L4','L5','L6','L7','L8','L9'),(14,10,25,100,500,100,12,40,24))),'wrong framework suite totals')
    return mapped

def verify_inventory(root, master_csv):
    folder = root / "docs" / "eval"
    decisions = read_csv(folder / "dispositions.csv", DECISIONS)
    for record in decisions:
        disposition = record["disposition"]
        check(not disposition or disposition in REASONS,
              key(record) + ": native result dispositions require a separate qualification")
    source = sources(root)
    rows = form(source, decisions)
    lookup = unique(rows, key, "source inventory")
    check(Counter(row["catalog"] for row in rows) == Counter(EXPECTED), "source counts differ")
    for identifier, row in lookup.items():
        relation = row["relation"]
        links = targets(row)
        check(relation in RELATIONS and links and all(link in lookup for link in links),
              identifier + ": invalid relation or missing target")
        check((relation == "SELF" and links == [identifier]) or
              (relation != "SELF" and identifier not in links), identifier + ": invalid self relation")
        disposition = row["disposition"]
        reason = row["reason_arg"]
        if not disposition:
            check("Pending" in row["notes"], identifier + ": pending reason is absent")
        elif disposition != "IDEA-BANK":
            check(bool(reason), identifier + ": empty disposition reason")
        if disposition in ("DUPLICATE-OF", "SUPERSEDED-BY"):
            check([ref.strip() for ref in reason.split(";")] == links, identifier + ": disposition target mismatch")
        if disposition == "LAW-CONFLICT":
            check(re.search(r"law\s+\d+", reason, re.I), identifier + ": missing law reference")
        if disposition == "NEEDS-OWNER":
            check(re.search(r"D-SB-\d+|D1\(a\)|D2|CAP-O|OD-25", reason), identifier + ": missing owner decision")
    # Reconstruct both shipped projections directly from the native catalogs.
    # A decision or scorecard cannot add a measured native verdict to these rows.
    check((folder / "TRACE.csv").read_bytes() == csv_data(rows),
          "TRACE.csv differs from the source-only inventory")
    check((folder / "TRACE.md").read_bytes() == markdown(rows),
          "TRACE.md differs from the source-only inventory")
    source_joins(source, rows)
    slots = framework_crosswalk(read_csv(master_csv), source, rows)
    dispositions = Counter(row["disposition"] or "Pending" for row in rows)
    return {"source_rows": len(rows), "framework_slots": len(slots),
            "pending": dispositions["Pending"], "native_verdicts": 0,
            "dispositions": dict(sorted(dispositions.items()))}


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    cli.add_argument("--master-csv", type=Path, help="Master Checklists CSV from the framework release")
    args = cli.parse_args()
    root = args.root.resolve()
    master = args.master_csv or root.parent / "ALife-Framework-v3.0-Master-Checklists.csv"
    print(json.dumps(verify_inventory(root, master), indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, StopIteration, csv.Error) as failure:
        print("Inventory check failed: " + str(failure), file=sys.stderr)
        sys.exit(1)
