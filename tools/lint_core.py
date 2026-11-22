#!/usr/bin/env python3
"""lint_core.py - development-time checks on the freestanding core.

This is tooling, not simulation code. It enforces the rules in
docs/ARCHITECTURE.md section 12 that a compiler cannot. The core is not built
with -mgeneral-regs-only (az_field.c and az_scan.c use GCC integer vector
types), so "no floating point, no mutable globals or statics, no libc" is
enforced here and, on the object code, by tools/check_core_obj.py.

  E1  no floating point types in core/   (float, double, long double tokens)
  E2  no libc / OS includes in core/     (only az_*.h, tk_*.h and the three freestanding headers)
  E3  no libc calls in core/             (malloc, printf, memcpy, ... ; az_* names are fine)
  E4  ASCII only, no tabs, LF line endings
  E5  no mutable file-scope statics      (static const is fine)
  E6  no floating literals               (1.5, 1e3, 2.f, .5, 1.0L, 0x1p3; 10u, 0xFFull are integers)
  E7  __builtin_* only from ALLOWED_BUILTINS (integer builtins the core used on 2026-11-17)
  E8  no non-static file-scope variable definitions (prototypes, extern, typedef,
      struct/enum/union definitions, _Static_assert stay allowed)
  E9  no function-local mutable statics  (static const is fine)
  W1  every trait AZ_T_* is read by the module core/az_owners.def names as its
      owner: by name, or through the phen scalar or the tag that carries it
  W2  every lesion bit AZ_LESION_* is honoured by each of its owner files
      (core/az_owners.def) and exercised in tests/

E1-E4 cover core/**/*.{c,h,def}; E5-E9 cover core/**/*.{c,h}. Comments and
string/char literals are blanked before every check except E2 (which reads
the include line itself) and E4.

Usage:  python lint_core.py <repo root> [--strict]
        python lint_core.py --self-test <repo root>
        --strict turns the W checks into errors (used for the final gate).
        --self-test lints the planted fixtures under tests/lint/<case>/core/
        (each must give exactly its rule's code on exactly its PLANT lines,
        the clean one nothing), synthesises E4 cases in memory, and lints the
        real tree, which must give 0 errors.
Exit code 0 = clean (or self-test passed).
"""
import os
import re
import sys

FREESTANDING_HEADERS = {"stdint.h", "stddef.h", "stdbool.h"}
LIBC_CALLS = [
    "malloc", "calloc", "realloc", "free", "printf", "fprintf", "sprintf", "snprintf", "puts",
    "memcpy", "memset", "memmove", "memcmp", "strlen", "strcpy", "strcmp", "abort", "exit",
    "rand", "srand", "time", "clock", "fopen", "fclose", "fread", "fwrite", "qsort", "sqrt",
    "sin", "cos", "exp", "log", "pow", "floor", "ceil", "abs", "labs", "assert",
]
# Integer builtins in use on 2026-11-17 (grep of core/*.c core/*.h core/stubs/*.c).
# Add a name here only for an integer-only builtin; never a math builtin.
ALLOWED_BUILTINS = {
    "__builtin_clz", "__builtin_clzll", "__builtin_ctz", "__builtin_expect",
    "__builtin_ia32_pmovmskb256", "__builtin_memcpy", "__builtin_offsetof",
    "__builtin_prefetch",   # a cache hint: reads nothing, changes no value (added 2026-11-18 for per-creature blocks)
    "__builtin_popcount", "__builtin_popcountll",
}
CALL_RE = re.compile(r"(?<![A-Za-z0-9_.>])(" + "|".join(LIBC_CALLS) + r")\s*\(")
FLOAT_RE = re.compile(r"\b(float|double)\b")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
STATIC_VAR_RE = re.compile(r"^static\s+(?!const\b)(?!inline\b)[A-Za-z_][A-Za-z0-9_ \*]*\s+\**[A-Za-z_][A-Za-z0-9_]*\s*(\[[^\]]*\])*\s*(=|;)")
PPNUM_RE = re.compile(r"(?<![A-Za-z0-9_.])(?:\d|\.\d)(?:[eEpP][+-]|[A-Za-z0-9_.])*")
BUILTIN_RE = re.compile(r"(?<![A-Za-z0-9_])(__builtin_[A-Za-z0-9_]+)")
PASTED_BUILTIN_RE = re.compile(r"__builtin_[A-Za-z0-9_]*\s*##|##\s*__builtin_")
CONST_RE = re.compile(r"\b(const|constexpr)\b")
SKIP_HEAD_RE = re.compile(r"^(typedef|extern|_Static_assert|static_assert|asm|__asm__|__asm|_Pragma)\b")
TAG_ONLY_RE = re.compile(r"^(struct|union|enum)\b(\s+[A-Za-z_][A-Za-z0-9_]*)?\s*(:[^{]*)?(\{\})?\s*$")
GROUP_NAMES = ("__attribute__", "_Alignas", "alignas", "__declspec", "_Atomic",
               "typeof", "__typeof__", "typeof_unqual", "__extension__")


def strip_comments_and_strings(text):
    """Blank out comments and string/char literals, keeping line structure."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + " " * max(0, j - i - 2) + (c if j - i >= 2 else ""))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def blank_preprocessor(code):
    """Blank preprocessor lines (with backslash continuations), keeping line count."""
    lines = code.split("\n")
    cont = False
    for k, line in enumerate(lines):
        if cont or line.lstrip().startswith("#"):
            cont = line.rstrip().endswith("\\")
            lines[k] = ""
    return "\n".join(lines)


def is_float_literal(tok):
    t = tok.lower()
    if t.startswith("0x"):
        return "." in t or "p" in t
    if t.startswith("0b"):
        return False
    return "." in t or "e" in t


def _close_of(s, k, open_ch, close_ch):
    """Index just past the group that opens at s[k]."""
    d = 0
    for j in range(k, len(s)):
        if s[j] == open_ch:
            d += 1
        elif s[j] == close_ch:
            d -= 1
            if d == 0:
                return j + 1
    return len(s)


def _remove_named_groups(s):
    for name in GROUP_NAMES:
        pat = re.compile(r"\b" + re.escape(name) + r"\b\s*")
        while True:
            m = pat.search(s)
            if not m:
                break
            end = m.end()
            if end < len(s) and s[end] == "(":
                end = _close_of(s, end, "(", ")")
            s = s[:m.start()] + " " + s[end:]
    return s


def _remove_groups(s, open_ch, close_ch):
    while True:
        k = s.find(open_ch)
        if k < 0:
            return s
        s = s[:k] + " " + s[_close_of(s, k, open_ch, close_ch):]


def _remove_param_lists(s):
    """Drop '(...)' groups that follow ')' : parameter lists of function pointers."""
    while True:
        m = re.search(r"\)\s*\(", s)
        if not m:
            return s
        k = m.end() - 1
        s = s[:k] + " " + s[_close_of(s, k, "(", ")"):]


def _top_level_cut(s, stops):
    d = 0
    for j, ch in enumerate(s):
        if ch in "([{":
            d += 1
        elif ch in ")]}":
            d -= 1
        elif d == 0 and ch in stops:
            return s[:j]
    return s


def head_is_function(head):
    """A '(' group makes a function unless the first group opens a pointer
    declarator such as (*fp)(void) or (*rows)[4]."""
    k = head.find("(")
    if k < 0:
        return False
    return not head[k + 1:].lstrip().startswith("*")


def head_is_const_object(head):
    h = _remove_param_lists(head)
    if "*" in h:
        return bool(CONST_RE.search(h[h.rfind("*") + 1:]))
    return bool(CONST_RE.search(h))


def structural_checks(code, rel):
    """E5 (chunk form), E8 and E9 on comment/string/preprocessor-blanked code."""
    found = []
    depth, paren0, line = 0, 0, 1
    chunk, chunk_line = [], None
    kind, body_start, body_line = None, 0, 1

    def analyze(text, first_line, last_line):
        s = " ".join(text.split())
        if not s:
            return
        s = _remove_named_groups(s).strip()
        if not s or SKIP_HEAD_RE.match(s):
            return
        is_static = bool(re.match(r"^(_Thread_local\s+|thread_local\s+)?static\b", s))
        core_s = re.sub(r"^(_Thread_local\s+|thread_local\s+)?static\b\s*", "", s) if is_static else s
        if TAG_ONLY_RE.match(_remove_groups(core_s, "{", "}").strip()):
            return
        head = _top_level_cut(core_s, "=,")
        head = _remove_groups(head, "{", "}")
        head = _remove_groups(head, "[", "]")
        head = " ".join(head.split())
        if head_is_function(head):
            return
        if head_is_const_object(head):
            return
        if is_static:
            found.append(("E5", first_line, last_line, "mutable file-scope static: " + s[:80]))
        else:
            found.append(("E8", first_line, last_line, "non-static file-scope variable: " + s[:80]))

    def body_statics(body, first_line):
        for m in re.finditer(r"\bstatic\b", body):
            before = body[:m.start()].rstrip()
            if before.endswith("["):
                continue
            end = body.find(";", m.start())
            decl = body[m.start():end if end >= 0 else len(body)]
            head = _remove_named_groups(_top_level_cut(decl, "=,"))
            head = " ".join(_remove_groups(head, "[", "]").split())
            if not head_is_const_object(head):
                ln = first_line + body.count("\n", 0, m.start())
                found.append(("E9", ln, ln, "function-local mutable static: " + " ".join(decl.split())[:80]))

    for i, c in enumerate(code):
        if depth == 0:
            if c == "{":
                s = "".join(chunk)
                st = _remove_named_groups(" ".join(s.split())).strip()
                if "=" in _strip_paren_text(s):
                    kind = "init"
                elif st.endswith(")") and not re.match(r"^(typedef|struct|union|enum)\b", st):
                    kind = "func"
                else:
                    kind = "agg"
                depth = 1
                body_start, body_line = i + 1, line
                if kind != "func":
                    chunk.append("{")
            elif c == ";" and paren0 == 0:
                analyze("".join(chunk), chunk_line, line)
                chunk, chunk_line = [], None
            else:
                if c == "(":
                    paren0 += 1
                elif c == ")":
                    paren0 -= 1
                if chunk_line is None and not c.isspace():
                    chunk_line = line
                chunk.append(c)
        else:
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    if kind == "func":
                        body_statics(code[body_start:i], body_line)
                        chunk, chunk_line = [], None
                    else:
                        chunk.append("}")
        if c == "\n":
            line += 1
    if depth != 0 or paren0 != 0 or "".join(chunk).strip():
        found.append(("E8", line, line, "unbalanced braces or unterminated declaration; file scope not fully checked"))
    return found


def _strip_paren_text(s):
    """Text at paren depth 0 only (so '=' inside attributes does not count)."""
    out, d = [], 0
    for ch in s:
        if ch == "(":
            d += 1
        elif ch == ")":
            d -= 1
        elif d == 0:
            out.append(ch)
    return "".join(out)


def lint_files(files):
    """files: list of (rel path, raw bytes). Returns (errors, code_by_file).

    Each error is (code, rel, line or None, message)."""
    errors = []
    code_by_file = {}
    for rel, raw in files:
        if b"\r" in raw:
            errors.append(("E4", rel, None, "CR line endings"))
        if b"\t" in raw:
            errors.append(("E4", rel, None, "tab character"))
        bad = [i for i, b in enumerate(raw) if b > 126 or (b < 32 and b not in (10, 13, 9))]
        if bad:
            line = raw.count(b"\n", 0, bad[0]) + 1
            errors.append(("E4", rel, line, f"non-ASCII or control byte 0x{raw[bad[0]]:02x}"))
        text = raw.decode("ascii", errors="replace")
        code = strip_comments_and_strings(text)
        code_by_file[rel] = code
        c_or_h = rel.endswith((".c", ".h"))
        for ln, (line, raw_line) in enumerate(zip(code.split("\n"), text.split("\n")), 1):
            if FLOAT_RE.search(line):
                errors.append(("E1", rel, ln, "floating point in core: " + raw_line.strip()[:80]))
            m = INCLUDE_RE.match(raw_line)
            if m:
                name = os.path.basename(m.group(2))
                angle = m.group(1) == "<"
                if angle and name not in FREESTANDING_HEADERS:
                    errors.append(("E2", rel, ln, f"include <{m.group(2)}> is not freestanding"))
                if not angle and not (name.startswith("az_") or name.startswith("tk_")):
                    errors.append(("E2", rel, ln, f'include "{m.group(2)}" is not a core header'))
            cm = CALL_RE.search(line)
            if cm:
                errors.append(("E3", rel, ln, f"libc call {cm.group(1)}(): " + raw_line.strip()[:80]))
            if not c_or_h:
                continue
            if (STATIC_VAR_RE.match(line) and "(" not in line.split("=")[0]
                    and not CONST_RE.search(line.split("=")[0])):
                errors.append(("E5", rel, ln, "mutable file-scope static: " + raw_line.strip()[:80]))
            if not m:
                fl = [t for t in PPNUM_RE.findall(line) if is_float_literal(t)]
                if fl:
                    errors.append(("E6", rel, ln, f"floating literal {fl[0]}: " + raw_line.strip()[:80]))
            for b in BUILTIN_RE.findall(line):
                if b not in ALLOWED_BUILTINS:
                    errors.append(("E7", rel, ln, f"builtin {b} is not in the integer allow-list: " + raw_line.strip()[:80]))
                    break
            # a builtin name glued together by the preprocessor cannot be checked
            # against the allow-list, so token pasting onto __builtin_ is refused
            # (the guard-check mutant `#define AZ_BI(x) __builtin_##x` got past)
            if PASTED_BUILTIN_RE.search(line):
                errors.append(("E7", rel, ln, "builtin name built by token pasting: " + raw_line.strip()[:80]))
        if c_or_h:
            e5_lines = {e[2] for e in errors if e[0] == "E5" and e[1] == rel}
            for code_id, first, last, msg in structural_checks(blank_preprocessor(code), rel):
                if code_id == "E5" and any(first <= l <= last for l in e5_lines):
                    continue
                errors.append((code_id, rel, first, msg))
    return errors, code_by_file


def collect_core(root):
    core = os.path.join(root, "core")
    files = []
    for dirpath, dirs, names in os.walk(core):
        dirs.sort()
        for f in sorted(names):
            if f.endswith((".c", ".h", ".def")):
                path = os.path.join(dirpath, f)
                rel = os.path.relpath(path, root).replace("\\", "/")
                with open(path, "rb") as fh:
                    files.append((rel, fh.read()))
    return files


OWNER_TRAIT_RE = re.compile(r"\bAZ_TRAIT\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)")
OWNER_LESION_RE = re.compile(r"\bAZ_LESION_OWNER\(\s*(\w+)\s*,\s*(\w+)\s*\)")
NOT_A_LESION_BIT = {"AZ_LESION_ALL"}


def has_name(code, name):
    return code is not None and re.search(r"\b" + name + r"\b", code) is not None


def coverage_check(abi, code_by_file, tests_code):
    """W1 and W2 as a pure function of the texts (ABI 4: driven by core/az_owners.def).

    W1: a trait counts as read when the file of its owner, core/az_<owner>.c,
    exists and contains the trait name (via ROW), the phen scalar that carries
    it (via AZ_P_*) or a use of the tag (via TAG). A trait with no owner line,
    or whose owner module has not been written, is unread.
    W2: a lesion bit counts as honoured when every one of its owner files
    exists and contains its name; it must also appear in a test."""
    out = []
    traits = sorted(set(re.findall(r"\bAZ_T_[A-Z0-9_]+\b", abi)) - {"AZ_T_COUNT"})
    lesions = sorted(set(re.findall(r"\bAZ_LESION_[A-Z0-9_]+\b", abi)) - NOT_A_LESION_BIT)
    owners = code_by_file.get("core/az_owners.def", "")
    t_owner = {"AZ_T_" + m.group(1): (m.group(2), m.group(3)) for m in OWNER_TRAIT_RE.finditer(owners)}
    l_owner = {}
    for m in OWNER_LESION_RE.finditer(owners):
        l_owner.setdefault("AZ_LESION_" + m.group(1), []).append(m.group(2))

    unread = []
    for t in traits:
        if t not in t_owner:
            unread.append(t + " (no owner line)")
            continue
        owner, via = t_owner[t]
        code = code_by_file.get(f"core/az_{owner}.c")
        if via == "ROW":
            ok = has_name(code, t)
        elif via == "MIND_CACHE":
            field = t.removeprefix("AZ_T_").lower()
            genome = code_by_file.get("core/az_genome.c", "")
            written = re.search(r"->" + field + r"\s*=\s*t\[" + t + r"\]", genome)
            read = re.search(r"history\s*->\s*" + field + r"\b", code or "")
            ok = bool(written and read)
        elif via == "TAG":
            ok = has_name(code, "tag") or has_name(code, "az_tag_dist")
        else:
            ok = via.startswith("AZ_P_") and has_name(code, via)
        if not ok:
            unread.append(f"{t} ({owner})")
    if unread:
        out.append(f"W1 {len(unread)} of {len(traits)} traits are not read by their owner module (core/az_owners.def): "
                   + ", ".join(unread[:12]) + (" ..." if len(unread) > 12 else ""))

    not_honoured = []
    for l in lesions:
        if l not in l_owner:
            not_honoured.append(l + " (no owner line)")
            continue
        missing = [o for o in l_owner[l] if not has_name(code_by_file.get(f"core/az_{o}.c"), l)]
        if missing:
            not_honoured.append(f"{l} ({', '.join(missing)})")
    no_test = [l for l in lesions if not has_name(tests_code, l)]
    if not_honoured:
        out.append("W2 lesion bits not honoured by their owner: " + ", ".join(not_honoured))
    if no_test:
        out.append("W2 lesion bits with no test: " + ", ".join(no_test))
    return out


def coverage_warnings(root, code_by_file):
    """W1 and W2. Skipped (empty) when the root has no core/az_abi.h (fixtures)."""
    abi_path = os.path.join(root, "core", "az_abi.h")
    if not os.path.isfile(abi_path):
        return []
    abi = open(abi_path, encoding="ascii", errors="replace").read()
    tests_dir = os.path.join(root, "tests")
    tests_code = ""
    if os.path.isdir(tests_dir):
        for f in sorted(os.listdir(tests_dir)):
            if f.endswith(".c"):
                tests_code += open(os.path.join(tests_dir, f), encoding="ascii", errors="replace").read()
    return coverage_check(abi, code_by_file, tests_code)


def coverage_self_test():
    """Planted cases for W1 and W2 (the fixtures on disk have no ABI header)."""
    abi = "enum { AZ_T_AA, AZ_T_BB, AZ_T_CC, AZ_T_DD, AZ_T_COUNT };\n" \
          "enum { AZ_LESION_XX = 1, AZ_LESION_YY = 2 };\n#define AZ_LESION_ALL 3\n"
    owners = "AZ_TRAIT(AA, one, ROW)\nAZ_TRAIT(BB, one, AZ_P_BB)\nAZ_TRAIT(CC, two, TAG)\nAZ_TRAIT(DD, two, ROW)\n" \
             "AZ_LESION_OWNER(XX, one)\nAZ_LESION_OWNER(YY, one)\nAZ_LESION_OWNER(YY, two)\n"
    good = {"core/az_owners.def": owners,
            "core/az_one.c": "x = t[AZ_T_AA] + p[AZ_P_BB]; if (l & AZ_LESION_XX) ; if (l & AZ_LESION_YY) ;",
            "core/az_two.c": "d = az_tag_dist(a, b) + t[AZ_T_DD]; if (l & AZ_LESION_YY) ;"}
    tests = "AZ_LESION_XX AZ_LESION_YY"
    cases = [("all read and honoured", good, tests, [])]
    bad = dict(good); bad["core/az_one.c"] = good["core/az_one.c"].replace("AZ_T_AA", "AZ_T_DD")
    cases.append(("a trait read only by a module that does not own it", bad, tests, ["W1 1 of 4"]))
    bad = dict(good); bad["core/az_one.c"] = good["core/az_one.c"].replace("AZ_P_BB", "AZ_P_ZZ")
    cases.append(("the phen scalar that carries a trait is not read", bad, tests, ["W1 1 of 4"]))
    cached = dict(good)
    cached["core/az_owners.def"] = owners.replace("AA, one, ROW","AA, one, MIND_CACHE")
    cached["core/az_one.c"] += " x=history->aa;"
    cached["core/az_genome.c"] = "sense_memory->aa = t[AZ_T_AA];"
    cases.append(("cached trait with development writer and owner read",cached,tests,[]))
    bad = dict(cached); bad["core/az_one.c"]=good["core/az_one.c"]
    cases.append(("raw trait token cannot replace the actual cache consumer",bad,tests,["W1 1 of 4"]))
    bad = dict(cached); bad["core/az_genome.c"]=""
    cases.append(("a cached trait without a development assignment",bad,tests,["W1 1 of 4"]))
    bad = dict(good); del bad["core/az_two.c"]
    cases.append(("an owner module that does not exist", bad, tests, ["W1 2 of 4", "W2 lesion bits not honoured"]))
    bad = dict(good); bad["core/az_two.c"] = good["core/az_two.c"].replace("AZ_LESION_YY", "0")
    cases.append(("one of two owners ignores a shared lesion", bad, tests, ["W2 lesion bits not honoured"]))
    bad = dict(good); bad["core/az_owners.def"] = owners.replace("AZ_TRAIT(AA, one, ROW)\n", "")
    cases.append(("a trait without an owner line", bad, tests, ["W1 1 of 4"]))
    cases.append(("a lesion no test mentions", good, "AZ_LESION_XX", ["W2 lesion bits with no test"]))
    fails = 0
    for name, files, tcode, want in cases:
        got = coverage_check(abi, files, tcode)
        ok = len(got) == len(want) and all(g.startswith(w) for g, w in zip(got, want))
        print(f"{'PASS' if ok else 'FAIL'} coverage: {name}: want {want or 'none'}, got {[g[:40] for g in got] or 'none'}")
        fails += 0 if ok else 1
    return fails


def fmt(e):
    code, rel, ln, msg = e
    return f"{code} {rel}:{ln}: {msg}" if ln is not None else f"{code} {rel}: {msg}"


def lint_tree(root, strict=False):
    files = collect_core(root)
    errors, code_by_file = lint_files(files)
    warnings = coverage_warnings(root, code_by_file)
    if strict:
        errors += [(w.split(" ", 1)[0], "core", None, w.split(" ", 1)[1]) for w in warnings]
        warnings = []
    return files, errors, warnings


# --- self-test -----------------------------------------------------------------
SELF_CASES = [
    ("e1_float_type", "E1"), ("e2_include", "E2"), ("e3_libc_call", "E3"),
    ("e5_file_static", "E5"), ("e6_float_literal", "E6"), ("e7_builtin", "E7"),
    ("e7_builtin_pasted", "E7"), ("e8_global", "E8"), ("e9_local_static", "E9"), ("clean", None),
]


def self_test(root):
    fails = 0
    base = os.path.join(root, "tests", "lint")
    clean_files = None
    for case, want in SELF_CASES:
        froot = os.path.join(base, case)
        files = collect_core(froot)
        if not files:
            print(f"FAIL {case}: no fixture files under {froot}/core")
            fails += 1
            continue
        if case == "clean":
            clean_files = files
        errors, _ = lint_files(files)
        planted = set()
        for rel, raw in files:
            for ln, l in enumerate(raw.decode("ascii", "replace").split("\n"), 1):
                if "PLANT" in l:
                    planted.add((rel, ln))
        codes = sorted({e[0] for e in errors})
        hit = {(e[1], e[2]) for e in errors}
        ok = True
        if want is None:
            ok = not errors and not planted
        else:
            ok = codes == [want] and hit == planted and len(planted) > 0
        print(f"{'PASS' if ok else 'FAIL'} {case}: want {want or 'none'}, got codes {codes or 'none'}, "
              f"{len(errors)} errors on {len(hit)} lines, {len(planted)} planted lines")
        if not ok:
            fails += 1
            for e in errors:
                print("    " + fmt(e))
            for p in sorted(planted - hit):
                print(f"    missed planted line {p[0]}:{p[1]}")
    # E4 has no fixture on disk (repo files must stay ASCII/LF/no tabs): synthesise it.
    if clean_files:
        rel, raw = clean_files[0]
        variants = [("e4_tab", raw.replace(b"\n", b"\n\t", 1)),
                    ("e4_crlf", raw.replace(b"\n", b"\r\n")),
                    ("e4_non_ascii", b"/* \xc3\xa9 */\n" + raw)]
        for name, data in variants:
            errors, _ = lint_files([(rel, data)])
            codes = sorted({e[0] for e in errors})
            ok = codes == ["E4"]
            print(f"{'PASS' if ok else 'FAIL'} {name} (in memory): want E4, got codes {codes or 'none'}")
            if not ok:
                fails += 1
                for e in errors:
                    print("    " + fmt(e))
    files, errors, warnings = lint_tree(root)
    # Tail probe: a global appended to each real .c/.h must be seen as E8, which
    # shows the brace walker reaches the end of every real file in a sane state.
    probe_fail = []
    for rel, raw in files:
        if not rel.endswith((".c", ".h")):
            continue
        last = raw.count(b"\n") + 2
        perrs, _ = lint_files([(rel, raw + b"\nunsigned az_lint_probe;\n")])
        if ("E8", last) not in {(e[0], e[2]) for e in perrs}:
            probe_fail.append(rel)
    ok = not probe_fail
    print(f"{'PASS' if ok else 'FAIL'} tail probe on real files: {len(probe_fail)} files missed the appended global")
    if not ok:
        fails += 1
        for rel in probe_fail:
            print("    " + rel)
    ok = not errors
    print(f"{'PASS' if ok else 'FAIL'} real tree: {len(files)} files, {len(errors)} errors, {len(warnings)} warnings")
    if not ok:
        fails += 1
        for e in errors:
            print("    " + fmt(e))
    fails += coverage_self_test()
    print(f"lint_core self-test: {'PASSED' if fails == 0 else 'FAILED'} ({fails} failing cases)")
    return 1 if fails else 0


def main():
    args = sys.argv[1:]
    if "--self-test" in args:
        rest = [a for a in args if a != "--self-test"]
        if len(rest) != 1:
            print(__doc__)
            return 2
        return self_test(rest[0])
    if not args or args[0].startswith("--"):
        print(__doc__)
        return 2
    root = args[0]
    strict = "--strict" in args[1:]
    files, errors, warnings = lint_tree(root, strict)
    for w in warnings:
        print("warning:", w)
    for e in errors:
        print("error:", fmt(e))
    print(f"lint_core: {len(files)} files, {len(errors)} errors, {len(warnings)} warnings"
          + (" (strict)" if strict else ""))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
