#!/usr/bin/env python3
"""编译、验证、测量，然后从原始记录生成两项实验的图表。"""
from pathlib import Path
import csv
import hashlib
import json
import os
import platform
import statistics
import subprocess
from datetime import datetime
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[2]
CODE = Path(__file__).resolve().parent
OUT = CODE.parent / '实验结果'
TMP = ROOT / 'temp'
os.environ.setdefault('MPLCONFIGDIR', str(TMP / 'matplotlib'))
os.environ.setdefault('XDG_CACHE_HOME', str(TMP / 'cache'))
import numpy as np
from PIL import Image, ImageDraw
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
plt.rcParams['font.family'] = FontProperties(
    fname='/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc').get_name()
plt.rcParams['axes.unicode_minus'] = False
LABELS = {'serial': '串行', 'static': '静态分块', 'lock': '锁任务池', 'atomic': '原子任务池'}
COLORS = {'serial': '#53677f', 'static': '#268880', 'lock': '#c48738', 'atomic': '#8967a8'}


def run(args, **kwargs):
    return subprocess.run(args, check=True, text=True, **kwargs)


def save(fig, name):
    fig.savefig(OUT / f'{name}.png', dpi=220, metadata={'Software': None})
    fig.savefig(OUT / f'{name}.pdf', metadata={
        'Author': '', 'Creator': '', 'Producer': '', 'CreationDate': None, 'ModDate': None})
    plt.close(fig)


def fixed_input():
    w = h = 4096
    x = np.arange(w, dtype=np.uint32)[None, :]
    y = np.arange(h, dtype=np.uint32)[:, None]
    image = Image.fromarray((25 + 110*x//(w-1) + 35*y//(h-1)).astype(np.uint8))
    d = ImageDraw.Draw(image)
    d.rectangle((300, 300, 1700, 1600), fill=215, outline=250, width=18)
    d.ellipse((2100, 350, 3650, 1900), fill=55, outline=245, width=24)
    d.polygon([(500, 3450), (1450, 2000), (2150, 3450)], fill=185, outline=255)
    for k in range(9):
        d.line((2400, 2300+115*k, 3650, 2500+115*k), fill=30+25*k, width=35)
    # 已有输入时先核对像素，防止不知情地更换实验数据。
    path = OUT / '固定输入灰度图.pgm'
    if path.exists():
        with Image.open(path) as old:
            if old.size != image.size or old.tobytes() != image.tobytes():
                raise RuntimeError('现有输入与固定场景不同，请先检查文件')
    image.save(path)
    image.save(OUT / '固定输入灰度图.png')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    OUT.mkdir(exist_ok=True)
    TMP.mkdir(exist_ok=True)
    digest = fixed_input()
    env = os.environ.copy()
    env.update(OMP_DYNAMIC='FALSE', OMP_PROC_BIND='close', OMP_PLACES='cores', OMP_WAIT_POLICY='PASSIVE')
    build = run(['make', '-j4', 'all', 'test', 'sanitize'], cwd=CODE, env=env,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (OUT / '编译与正确性验证.txt').write_text(build.stdout)
    bench = run([str(TMP/'bin/benchmark'), str(OUT/'固定输入灰度图.pgm'),
                 str(OUT/'原始计时数据.csv'), str(OUT)], env=env,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (OUT / '正确性验证.txt').write_text(bench.stdout)
    modes = ['serial'] + [f'{m}_{p}' for m in ['static', 'lock', 'atomic'] for p in [1, 2, 4]]
    for mode in modes:
        with Image.open(OUT/f'{mode}.pgm') as im:
            im.save(OUT/f'{mode}.png')
        (OUT/f'{mode}.pgm').replace(TMP/f'{mode}.pgm')
    raw = list(csv.DictReader((OUT/'原始计时数据.csv').open()))
    stats = []
    for mode in ['serial', 'static', 'lock', 'atomic']:
        for p in ([1] if mode == 'serial' else [1, 2, 4]):
            times = [float(r['core_ms']) for r in raw if r['mode'] == mode and int(r['threads']) == p]
            assert len(times) == 30
            stats.append(dict(mode=mode, threads=p, runs=len(times), mean_ms=statistics.mean(times),
                              std_ms=statistics.stdev(times), median_ms=statistics.median(times),
                              min_ms=min(times), max_ms=max(times)))
    base = stats[0]['mean_ms']
    for s in stats:
        s.update(speedup=base/s['mean_ms'], efficiency=base/s['mean_ms']/s['threads'])
    with (OUT/'性能测试数据.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=stats[0].keys(), lineterminator="\n")
        writer.writeheader()
        writer.writerows(stats)
    sources = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
               for p in CODE.iterdir() if p.suffix in ['.cpp', '.hpp'] or p.name == 'Makefile'}
    metadata = dict(date=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(),
                    compiler=run(['g++', '--version'], stdout=subprocess.PIPE).stdout.splitlines()[0],
                    os=platform.platform(), cpu=run(['lscpu'], stdout=subprocess.PIPE).stdout,
                    cpu_max=Path('/sys/fs/cgroup/cpu.max').read_text().strip(),
                    memory_max=Path('/sys/fs/cgroup/memory.max').read_text().strip(),
                    affinity=sorted(os.sched_getaffinity(0)), input_sha256=digest, input_size=[4096, 4096],
                    block_rows=64, task_count=64, warmups=3, repeats=30, seed=20261025,
                    flags='-O3 -std=c++17 -fopenmp -Wall -Wextra -Wpedantic',
                    openmp_env={k: env[k] for k in ['OMP_DYNAMIC', 'OMP_PROC_BIND', 'OMP_PLACES', 'OMP_WAIT_POLICY']},
                    cpu_stat=(OUT/'配额计数.txt').read_text(), sources_sha256=sources, stats=stats)
    (OUT/'实验元数据.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2))

    chosen = [s for s in stats if s['mode'] == 'serial' or s['threads'] == 4]
    fig, ax = plt.subplots(figsize=(7.2, 4), layout='constrained')
    ax.bar([LABELS[s['mode']] for s in chosen], [s['mean_ms'] for s in chosen],
           yerr=[s['std_ms'] for s in chosen], capsize=5, color=[COLORS[s['mode']] for s in chosen])
    ax.set(ylabel='核心计算平均时间 / ms', title='实验一：串行与三种4线程方案（均值±样本标准差）')
    ax.set_ylim(0, max(s['mean_ms']+s['std_ms'] for s in chosen)*1.25)
    for i, s in enumerate(chosen):
        ax.text(i, s['mean_ms']+s['std_ms']+1, f"{s['mean_ms']:.3f}", ha='center')
    save(fig, '实验一_方案对比')

    fig, axes = plt.subplots(1, 3, figsize=(12, 3.7), layout='constrained')
    for mode in ['static', 'lock', 'atomic']:
        group = [s for s in stats if s['mode'] == mode]
        axes[0].errorbar([1, 2, 4], [s['mean_ms'] for s in group],
                         yerr=[s['std_ms'] for s in group], capsize=4,
                         marker='o', color=COLORS[mode], label=LABELS[mode])
        for ax, key in [(axes[1], 'speedup'), (axes[2], 'efficiency')]:
            ax.plot([1, 2, 4], [s[key] for s in group], '-o', color=COLORS[mode], label=LABELS[mode])
    axes[0].axhline(base, color=COLORS['serial'], linestyle='--', label='串行均值')
    axes[1].plot([1, 2, 4], [1, 2, 4], '--', color='#999999', label='理想加速比')
    for ax, ylabel in zip(axes, ['核心时间 / ms', '相对串行加速比', '并行效率']):
        ax.set(xlabel='线程数量', ylabel=ylabel, xticks=[1, 2, 4])
        ax.legend(fontsize=8)
        ax.grid(alpha=.2)
    save(fig, '实验二_线程扩展')

    diag = list(csv.DictReader((OUT/'线程诊断数据.csv').open()))
    fig, axes = plt.subplots(1, 3, figsize=(10, 3.3), layout='constrained')
    for ax, mode in zip(axes, ['static', 'lock', 'atomic']):
        rows = [r for r in diag if r['mode'] == mode and int(r['threads']) == 4 and int(r['run']) == 1]
        ax.bar([int(r['thread_id']) for r in rows], [int(r['tasks']) for r in rows], color=COLORS[mode])
        ax.set(xlabel='线程编号', ylabel='完成任务数', title=LABELS[mode], xticks=range(4), ylim=(0, 32))
    save(fig, '诊断_任务分配')

    fig, axes = plt.subplots(1, 4, figsize=(11, 3.2), layout='constrained')
    for ax, name, title in zip(axes, ['固定输入灰度图.png', 'static_4.png', 'lock_4.png', 'atomic_4.png'],
                                ['固定输入', '静态分块', '锁任务池', '原子任务池']):
        ax.imshow(Image.open(OUT/name), cmap='gray', vmin=0, vmax=255)
        ax.set_title(title)
        ax.axis('off')
    fig.savefig(OUT/'输入与输出对比.png', dpi=220, metadata={'Software': None})
    plt.close(fig)
    print(json.dumps(stats, ensure_ascii=False, indent=2))
    print('Input SHA256:', digest)

if __name__ == '__main__':
    main()
