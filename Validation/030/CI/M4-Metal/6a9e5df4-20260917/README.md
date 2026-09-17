# M4 validation at the SDR reference-provenance freeze

Tested software: `6a9e5df4be780b31a8e3a9ec3ee18b35f0512838`. The clean detached software checkout remained separate from this evidence branch. Strict Release and fresh Lantern/Sponza Metal arm64 cooks passed; both publications passed `validate --strict-files`. Pinned Sponza source verification covered 71 files / 52,686,624 bytes. Build, binary and cook log identities are in `preflight.json`; raw commands/logs, cooked packages and raw captures remain local.

All fourteen prescribed M4 cases passed on their first attempt through `interactive_lab_validation.py run` and independent `verify`: seven smoke, three endurance and four lifecycle. The five 1120-frame cases and nine 120-frame cases retain the fixed Coverage-v2 budgets. Sponza lifecycle completed 180 steps, Lantern mode transitions 100, and the remaining integrations 9/5. Every case has zero capture-disabled readback copies/maps/waits, live queue/device idles and final native/presentation owners. Metal shutdown remains NativeCallback/Proven. No physical scanout claim is added.

Independent consumption also verified the thirteen hosted records from run 35193398574 and all five current Windows RTX 3080 records. Windows RDP and forced AcquireHistory / IdleAssumed limitations remain unchanged. T116–T118 are complete at this SHA. Historical `74e1c435` first mode-transition failure and unchanged passing retry are retained under `Validation/030/CI/History/74e1c435/M4-Metal/`, exclusively as history.

The maintainer reviewed current live Lantern and Sponza windows and explicitly accepted the navigation/input/preset/lifecycle checklist and all four HDR scene/UI modes per workload. Lantern words: “gud”, confirmed as “是，全部已检查并接受”. Sponza words: “全部已检查并接受”. Ten decisions preserve those replies and bind current request digests; the full question scope and live-session originals are retained under `Validation/030/UI/M4-Metal/6a9e5df4-20260917/Human/`. Both live sessions exited normally. The initial Lantern launcher used a protected validation directory for presets and was rejected before any frame; its original report remains separate. The corrected local launcher used `Build/InteractiveLab`, without changing software or policy. No Windows decision is inferred from the M4 answers.

Both M4 workloads passed three independent processes × twenty captures, required calibration mutations, current native probe/Candidate provenance and exact 512×512 comparison: zero differing channels against existing Accepted. Sponza required one unchanged fresh-directory retry after its retained first failure. Strict binding still rejects both current capability digests against historical Accepted identities; the reference-path fix does not relax this identity check. Formal SDR results and the strict binding rejection are recorded in `checkpoint.json`, `formal-closeout-result.json` and `Validation/030/SDR/formal-parity.json`. The first Sponza calibration failed at native presentation recovery preflight; its bounded diagnostic and original log digest are in `sponza-formal-first-attempt.json`. Successful reruns never erase that attempt. Accepted records and tolerances are unchanged. Intel macOS stays skipped.

## Reproduce independent consumption

`inventory.json` links bounded machine/human/formal/external inventory shards. Each maps original repository-relative paths to stored immutable artifacts and their SHA-256 values. The external shard includes dependencies already archived with hosted/Windows evidence. Rehydrate all originals without rewriting JSON:

```python
import hashlib, json
from pathlib import Path
root = Path.cwd()
index = json.loads((root / 'Validation/030/CI/M4-Metal/6a9e5df4-20260917/inventory.json').read_text())
items = []
for shard in index['inventories']:
    payload = (root / shard['path']).read_bytes()
    assert hashlib.sha256(payload).hexdigest() == shard['sha256']
    items.extend(json.loads(payload)['artifacts'])
for item in items:
    source = root / (item.get('path') or item['storedPath'])
    data = source.read_bytes()
    assert hashlib.sha256(data).hexdigest() == item['sha256']
    assert len(data) == item['sizeBytes']
    destination = root / item['originalPath']
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        assert destination.read_bytes() == data
    else:
        destination.write_bytes(data)
```

Use the exact frozen validator against this evidence checkout:

```sh
python .github/scripts/interactive_lab_validation.py closeout \
  --root . --manifest Validation/030/CI/partial-bundle.json \
  --git-revision 6a9e5df4be780b31a8e3a9ec3ee18b35f0512838
```

The partial bundle includes only verified gates and remains incomplete. `formal-attempt-bundle.json` separately preserves the attempted strict formal linkage and its rejection. A structural or machine pass never substitutes for missing current formal or human authority.
