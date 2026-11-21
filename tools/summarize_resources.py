"""Summarize measure_resources.ps1 CSV; accepts decimal-point or Windows decimal-comma output."""
import csv
import json
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path

folder = Path(sys.argv[1])
groups = defaultdict(list)
for row in csv.DictReader((folder / 'samples.csv').open(encoding='utf-8-sig', newline='')):
    for key, value in list(row.items()):
        if key not in ('utc', 'phase', 'responding', 'window_visible', 'window_minimized', 'window_foreground'):
            row[key] = float(value.replace(',', '.')) if value else None
    groups[row['phase']].append(row)

def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]

report = []
for phase, all_rows in groups.items():
    # Trim both edges: setup and the action that changes to the next phase.
    rows = [r for r in all_rows if all_rows[0]['elapsed_s'] + 5 <= r['elapsed_s'] <= all_rows[-1]['elapsed_s'] - 5]
    before_state_filter = len(rows)
    if rows and 'window_visible' in rows[0]:
        if phase.endswith('_minimized'):
            rows = [r for r in rows if r['window_minimized'].lower() == 'true']
        elif phase.endswith('_tray'):
            rows = [r for r in rows if r['window_visible'].lower() == 'false']
        elif phase.endswith(('_static', '_animated')):
            rows = [r for r in rows if r['window_visible'].lower() == 'true' and r['window_minimized'].lower() == 'false']
    if not rows:
        continue
    result = dict(phase=phase, samples=len(rows), span_s=round(rows[-1]['elapsed_s'] - rows[0]['elapsed_s'], 1),
                  unresponsive_samples=sum(r['responding'].lower() != 'true' for r in rows), discarded_state_samples=before_state_filter-len(rows))
    if 'window_visible' in rows[0]:
        result['window_states'] = dict(visible_samples=sum(r['window_visible'].lower() == 'true' for r in rows),
                                      minimized_samples=sum(r['window_minimized'].lower() == 'true' for r in rows),
                                      foreground_samples=sum(r['window_foreground'].lower() == 'true' for r in rows))
    for metric in ('cpu_pct', 'working_set_mib', 'private_mib', 'gpu_pct', 'gpu_dedicated_mib', 'gpu_shared_mib', 'handles', 'threads', 'gdi_objects', 'user_objects'):
        values = [r[metric] for r in rows if r[metric] is not None]
        if not values:
            result[metric] = None
            continue
        window = min(15, len(values))
        result[metric] = dict(mean=round(statistics.mean(values), 3), p95=round(percentile(values, .95), 3),
                              maximum=round(max(values), 3), first=round(statistics.median(values[:window]), 3),
                              last=round(statistics.median(values[-window:]), 3), valid_samples=len(values))
    report.append(result)
(folder / 'summary.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
