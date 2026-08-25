import sys

SRC = "/mnt/user-data/uploads/forplan.mps"
DST = "/home/claude/work/forplan_fixed.mps"

def f(line, a, b):
    s = line[a:b] if len(line) >= a else ""
    return s.strip()

raw = open(SRC, "rb").read().decode("ascii")
lines = raw.split("\r\n")
if lines and lines[-1] == "":
    lines.pop()

# locate sections
sec_idx = {}
for i, l in enumerate(lines):
    if l and not l.startswith(" ") and not l.startswith("*"):
        sec_idx[l.split()[0] if not l.startswith("NAME") else "NAME"] = i

name_line_idx = 0
rows_idx = sec_idx["ROWS"]
cols_idx = sec_idx["COLUMNS"]
rhs_idx = sec_idx["RHS"]
ranges_idx = sec_idx.get("RANGES")
bounds_idx = sec_idx["BOUNDS"]
endata_idx = sec_idx["ENDATA"]

# ---- parse ROWS ----
rows = []  # (type, rawname)
for l in lines[rows_idx+1:cols_idx]:
    typ = f(l, 1, 3)
    rname = f(l, 4, 12)
    rows.append((typ, rname))

# ---- parse COLUMNS ----
col_entries = []  # (colname, rowname, valuestr)
col_order = []
seen_cols = set()
for l in lines[cols_idx+1:rhs_idx]:
    cname = f(l, 4, 12)
    if cname:
        if cname not in seen_cols:
            seen_cols.add(cname)
            col_order.append(cname)
    r1 = f(l, 14, 22)
    v1 = f(l, 24, 36)
    if r1:
        col_entries.append((cname, r1, v1))
    r2 = f(l, 39, 47)
    v2 = f(l, 49, 61)
    if r2:
        col_entries.append((cname, r2, v2))

# ---- parse RHS ----
rhs_entries = []  # (rowname, valuestr)
for l in lines[rhs_idx+1:(ranges_idx if ranges_idx else bounds_idx)]:
    r1 = f(l, 14, 22)
    v1 = f(l, 24, 36)
    if r1:
        rhs_entries.append((r1, v1))
    r2 = f(l, 39, 47)
    v2 = f(l, 49, 61)
    if r2:
        rhs_entries.append((r2, v2))

# ---- parse RANGES ----
range_entries = []  # (rowname, valuestr)
if ranges_idx:
    for l in lines[ranges_idx+1:bounds_idx]:
        r1 = f(l, 14, 22)
        v1 = f(l, 24, 36)
        if r1:
            range_entries.append((r1, v1))
        r2 = f(l, 39, 47)
        v2 = f(l, 49, 61)
        if r2:
            range_entries.append((r2, v2))

# ---- parse BOUNDS ----
bound_entries = []  # (label, colname, valuestr or None)
for l in lines[bounds_idx+1:endata_idx]:
    label = f(l, 1, 3)
    cname = f(l, 14, 22)
    val = f(l, 24, 36)
    if not label:
        continue
    bound_entries.append((label, cname, val if val else None))

print("rows:", len(rows), "cols:", len(col_order), "col_entries:", len(col_entries),
      "rhs:", len(rhs_entries), "ranges:", len(range_entries), "bounds:", len(bound_entries))

# ---- sanitize names (strip embedded spaces) ----
def sanitize(n):
    return n.replace(" ", "")

row_name_map = {}
for typ, rname in rows:
    row_name_map[rname] = sanitize(rname)

col_name_map = {}
for cname in col_order:
    col_name_map[cname] = sanitize(cname)

# sanity: check sanitized names still unique
rvals = list(row_name_map.values())
assert len(rvals) == len(set(rvals)), "row name collision after sanitizing"
cvals = list(col_name_map.values())
assert len(cvals) == len(set(cvals)), "col name collision after sanitizing"

# ---- apply RANGES -> split rows ----
# find row types
row_type = {rname: typ for typ, rname in rows}
rhs_val = {rname: val for rname, val in rhs_entries}

extra_rows = []      # (type, newrowname)
extra_col_entries = []  # (colname, newrowname, valuestr)
extra_rhs = []        # (newrowname, valuestr)

used_names = set(row_name_map.values())

def make_unique(base):
    cand = base
    i = 0
    while cand in used_names:
        i += 1
        cand = (base[:6] + str(i)) if len(base) >= 6 else (base + str(i))
    used_names.add(cand)
    return cand

for rname, rangeval in range_entries:
    typ = row_type[rname]
    r = float(rangeval)
    rhs = float(rhs_val.get(rname, "0"))
    san = row_name_map[rname]

    if typ == "E":
        if r >= 0:
            lo, hi = rhs, rhs + r
        else:
            lo, hi = rhs + r, rhs
    elif typ == "L":
        lo, hi = rhs - abs(r), rhs
    elif typ == "G":
        lo, hi = rhs, rhs + abs(r)
    else:
        raise ValueError("unexpected row type for RANGES: " + typ)

    if typ == "G":
        # existing row already encodes: row >= lo (rhs=lo). Add new L row for upper bound hi.
        new_name = make_unique(san[:7] + "U" if len(san) < 8 else san[:7] + "U")
        extra_rows.append(("L", new_name))
        extra_rhs.append((new_name, repr(hi)))
        for (cn, rn, v) in col_entries:
            if rn == rname:
                extra_col_entries.append((cn, new_name, v))
        # keep original row's RHS as lo (already is, since G row uses rhs as-is)
    elif typ == "L":
        # existing row already encodes: row <= hi (rhs=hi). Add new G row for lower bound lo.
        new_name = make_unique(san[:7] + "L" if len(san) < 8 else san[:7] + "L")
        extra_rows.append(("G", new_name))
        extra_rhs.append((new_name, repr(lo)))
        for (cn, rn, v) in col_entries:
            if rn == rname:
                extra_col_entries.append((cn, new_name, v))
    elif typ == "E":
        # convert original E row itself into an L row at hi, and add a new G row at lo
        row_type[rname] = "L"
        # need to update rows list type too
        new_name = make_unique(san[:7] + "L" if len(san) < 8 else san[:7] + "L")
        extra_rows.append(("G", new_name))
        extra_rhs.append((new_name, repr(lo)))
        for (cn, rn, v) in col_entries:
            if rn == rname:
                extra_col_entries.append((cn, new_name, v))
        # update rhs of original E(now L) row to hi
        rhs_val[rname] = repr(hi)

print("extra_rows:", extra_rows)

# ---- write output ----
out = []
out.append("NAME          FORPLAN")
out.append("ROWS")
for typ, rname in rows:
    t = row_type[rname]  # possibly updated (E->L) though not needed here
    out.append(" %s  %s" % (t, row_name_map[rname]))
for typ, new_name in extra_rows:
    out.append(" %s  %s" % (typ, new_name))

out.append("COLUMNS")
# group extra col entries by (colname, original row) for quick lookup
extra_by_key = {}
for (cn, newrow, v) in extra_col_entries:
    extra_by_key.setdefault(cn, []).append((newrow, v))

# figure out which original (col,row) pairs trigger an insertion (only need cn match,
# since we only have one ranged row in this file; insert right after we hit that col's
# entry for the ranged row)
ranged_rownames = set(r for r, _ in range_entries)

for (cn, rn, v) in col_entries:
    out.append(" %s  %s  %s" % (col_name_map[cn], row_name_map[rn], v))
    if rn in ranged_rownames:
        for (newrow, val) in extra_by_key.get(cn, []):
            out.append(" %s  %s  %s" % (col_name_map[cn], newrow, val))

out.append("RHS")
for rn, v in rhs_entries:
    v_use = rhs_val.get(rn, v)
    out.append("    RHS  %s  %s" % (row_name_map[rn], v_use))
for newrow, v in extra_rhs:
    out.append("    RHS  %s  %s" % (newrow, v))

out.append("BOUNDS")
for label, cname, val in bound_entries:
    if val is not None:
        out.append(" %s BND  %s  %s" % (label, col_name_map[cname], val))
    else:
        out.append(" %s BND  %s" % (label, col_name_map[cname]))

out.append("ENDATA")

open(DST, "w", newline="\r\n").write("\n".join(out) + "\n")
print("wrote", DST, "lines:", len(out))
