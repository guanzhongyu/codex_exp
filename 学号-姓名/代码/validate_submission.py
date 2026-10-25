#!/usr/bin/env python3
"""核对原始数据、汇总表、结果图和报告是否属于同一次实验。"""
from pathlib import Path
import csv
import hashlib
import json
import statistics
import zipfile
import math
import fitz
import numpy as np
from PIL import Image
from docx import Document

code = Path(__file__).resolve().parent
root = code.parents[1]
submission = code.parent
out = submission/'实验结果'
raw = list(csv.DictReader((out/'原始计时数据.csv').open()))
summary = list(csv.DictReader((out/'性能测试数据.csv').open()))
meta = json.loads((out/'实验元数据.json').read_text())
assert len(raw) == 300 and len(summary) == 10
assert all(int(r['mismatch_pixels']) == 0 and r['block_rows'] == '64' for r in raw)
for round_id in range(1,31):
    group = [r for r in raw if int(r['round']) == round_id]
    assert len(group) == 10 and {int(r['order']) for r in group} == set(range(1,11))
    assert len({(r['mode'],r['threads']) for r in group}) == 10
base = float(summary[0]['mean_ms'])
for s in summary:
    times = [float(r['core_ms']) for r in raw if (r['mode'],r['threads']) == (s['mode'],s['threads'])]
    assert len(times) == int(s['runs']) == 30
    for field,value in [('mean_ms',statistics.mean(times)),('std_ms',statistics.stdev(times)),
                        ('median_ms',statistics.median(times)),('min_ms',min(times)),('max_ms',max(times))]:
        assert math.isclose(float(s[field]),value,rel_tol=1e-10)
    assert math.isclose(float(s['speedup']),base/float(s['mean_ms']))
    assert math.isclose(float(s['efficiency']),float(s['speedup'])/int(s['threads']))
for name,digest in meta['sources_sha256'].items():
    assert hashlib.sha256((code/name).read_bytes()).hexdigest() == digest
assert hashlib.sha256((out/'固定输入灰度图.pgm').read_bytes()).hexdigest() == meta['input_sha256']
with Image.open(out/'serial.png') as im:
    reference = im.tobytes()
    pixels = np.array(im)
    assert im.size == (4096,4096)
assert not pixels[0].any() and not pixels[-1].any() and not pixels[:,0].any() and not pixels[:,-1].any()
for mode in ['static','lock','atomic']:
    for p in [1,2,4]:
        with Image.open(out/f'{mode}_{p}.png') as im:
            assert im.size == (4096,4096) and im.tobytes() == reference
rows = list(csv.DictReader((out/'线程诊断数据.csv').open()))
assert len(rows) == 63
for repeat in range(1,4):
    for mode in ['static','lock','atomic']:
        for p in [1,2,4]:
            group = [r for r in rows if int(r['run']) == repeat and r['mode'] == mode and int(r['threads']) == p]
            assert len(group) == p and all(int(r['actual_threads']) == p for r in group)
            assert sum(int(r['tasks']) for r in group) == 64
            assert sum(int(r['rows']) for r in group) == 4094
            assert sum(int(r['claims']) for r in group) == (0 if mode == 'static' else 64+p)
report = submission/'报告/程序设计报告.docx'
doc = Document(report)
assert not doc.core_properties.author and not doc.core_properties.last_modified_by
with zipfile.ZipFile(report) as z:
    assert 'docProps/custom.xml' not in z.namelist()
for path in submission.rglob('*.pdf'):
    with fitz.open(path) as pdf:
        assert not pdf.metadata.get('author')
        if path.name == '程序设计报告.pdf':
            text = ''.join(page.get_text() for page in pdf)
            for value in summary:
                assert f"{float(value['mean_ms']):.3f}" in text
            for word in ['2695','2640','300','PCAM','atomic','临界区','诊断']:
                assert word in text
            assert all(len(page.get_text().strip()) > 100 for page in pdf)
            print('Report pages:',len(pdf))
print('PASS: 300 timing rows, 10 aggregates, source and input hashes, all outputs, 63 diagnostic rows, report values and blank author metadata')
