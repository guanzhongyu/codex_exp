#!/usr/bin/env python3
"""Fixed input, two experiments, all original measurements retained."""
from pathlib import Path
import os, subprocess, hashlib, json, csv, statistics, platform
os.environ.setdefault('MPLCONFIGDIR',str(Path(__file__).resolve().parents[2]/'temp/matplotlib'))
os.environ.setdefault('XDG_CACHE_HOME',str(Path(__file__).resolve().parents[2]/'temp/cache'))
import numpy as np
from PIL import Image, ImageDraw
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
ROOT=Path(__file__).resolve().parents[2]
CODE=Path(__file__).resolve().parent
OUT=CODE.parent/'实验结果'
TMP=ROOT/'temp'
OUT.mkdir(exist_ok=True); TMP.mkdir(exist_ok=True)
font=FontProperties(fname='/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc')
plt.rcParams['font.family']=font.get_name()
plt.rcParams['axes.unicode_minus']=False

def run(cmd,**kw):
    return subprocess.run(cmd,check=True,text=True,**kw)

def main():
    # One fixed deterministic scene; all timing groups use this exact file.
    w=h=4096
    x=np.arange(w,dtype=np.uint32)[None,:]
    y=np.arange(h,dtype=np.uint32)[:,None]
    a=(25+110*x//(w-1)+35*y//(h-1)).astype(np.uint8)
    image=Image.fromarray(a)
    d=ImageDraw.Draw(image)
    d.rectangle((300,300,1700,1600),fill=215,outline=250,width=18)
    d.ellipse((2100,350,3650,1900),fill=55,outline=245,width=24)
    d.polygon([(500,3450),(1450,2000),(2150,3450)],fill=185,outline=255)
    for k in range(9):
        d.line((2400,2300+115*k,3650,2500+115*k),fill=30+25*k,width=35)
    image.save(OUT/'固定输入灰度图.pgm'); image.save(OUT/'固定输入灰度图.png')
    digest=hashlib.sha256((OUT/'固定输入灰度图.pgm').read_bytes()).hexdigest()
    env=os.environ.copy();env.update(OMP_DYNAMIC='FALSE',OMP_PROC_BIND='close',OMP_PLACES='cores',OMP_WAIT_POLICY='PASSIVE')
    build=run(['make','-j','4','all','test','sanitize'],cwd=CODE,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (OUT/'编译与正确性验证.txt').write_text(build.stdout)
    before=Path('/sys/fs/cgroup/cpu.stat').read_text()
    bench=run([str(TMP/'bin/benchmark'),str(OUT/'固定输入灰度图.pgm'),str(OUT/'原始计时数据.csv'),str(OUT)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    after=Path('/sys/fs/cgroup/cpu.stat').read_text()
    (OUT/'正确性验证.txt').write_text(bench.stdout+'\nEvery timed output compared byte-for-byte with serial reference.\n')
    for mode in ['serial','parallel_1','parallel_2','parallel_4']:
        with Image.open(OUT/(mode+'.pgm')) as im:im.save(OUT/(mode+'.png'))
        # Binary intermediates are not submission artifacts.
        (OUT/(mode+'.pgm')).replace(TMP/(mode+'.pgm'))
    rows=list(csv.DictReader((OUT/'原始计时数据.csv').open()))
    stats=[]
    for mode,p in [('serial',1),('parallel',1),('parallel',2),('parallel',4)]:
        values=[float(r['core_ms']) for r in rows if r['mode']==mode and int(r['threads'])==p]
        stats.append(dict(mode=mode,threads=p,runs=len(values),mean_ms=statistics.mean(values),std_ms=statistics.stdev(values),min_ms=min(values),max_ms=max(values)))
    base=stats[0]['mean_ms']
    for s in stats:s.update(speedup=base/s['mean_ms'],efficiency=base/s['mean_ms']/s['threads'])
    with (OUT/'性能测试数据.csv').open('w',newline='') as f:
        wr=csv.DictWriter(f,fieldnames=stats[0].keys());wr.writeheader();wr.writerows(stats)
    info=dict(date='2026-10-25',os=platform.platform(),compiler=run(['g++','--version'],stdout=subprocess.PIPE).stdout.splitlines()[0],cpu=run(['lscpu'],stdout=subprocess.PIPE).stdout,cpu_max=Path('/sys/fs/cgroup/cpu.max').read_text().strip(),memory_max=Path('/sys/fs/cgroup/memory.max').read_text().strip(),affinity=sorted(os.sched_getaffinity(0)),input_sha256=digest,input_size=[w,h],warmups=3,repeats=30,random_seed=20261025,flags='-O3 -std=c++17 -fopenmp -Wall -Wextra -Wpedantic',openmp_env={k:env[k] for k in ['OMP_DYNAMIC','OMP_PROC_BIND','OMP_PLACES','OMP_WAIT_POLICY']},cpu_stat_before=before,cpu_stat_after=after,stats=stats)
    (OUT/'实验元数据.json').write_text(json.dumps(info,ensure_ascii=False,indent=2))
    # Experiment 1: means and sample standard deviations.
    fig,ax=plt.subplots(figsize=(6.8,4.1),layout='constrained')
    chosen=[stats[0],stats[3]]
    ax.bar(['串行','OpenMP 4线程'],[s['mean_ms'] for s in chosen],yerr=[s['std_ms'] for s in chosen],capsize=5,color=['#566c86','#268880'])
    ax.set_ylabel('核心计算平均时间 / ms');ax.set_title('实验一：串行与4线程对比（30次，误差线为标准差）',fontsize=11)
    for i,s in enumerate(chosen):ax.text(i,s['mean_ms']+s['std_ms']+0.5,f"{s['mean_ms']:.3f} ms",ha='center')
    ax.set_ylim(0,max(s['mean_ms']+s['std_ms'] for s in chosen)*1.2)
    for ext in ['png','pdf']:fig.savefig(OUT/f'实验一_时间对比.{ext}',dpi=200)
    plt.close(fig)
    fig,axs=plt.subplots(1,2,figsize=(10,4),layout='constrained')
    ps=[1,2,4];parallel=stats[1:]
    axs[0].errorbar(ps,[s['mean_ms'] for s in parallel],yerr=[s['std_ms'] for s in parallel],marker='o',capsize=5,color='#268880',label='OpenMP 平均值±标准差')
    axs[0].axhline(base,linestyle='--',color='#566c86',label='串行均值')
    axs[0].set(xlabel='线程数量',ylabel='核心计算时间 / ms',title='实验二：线程数量与时间');axs[0].legend(fontsize=8)
    axs[1].plot(ps,[s['speedup'] for s in parallel],'-o',color='#268880',label='实测加速比')
    axs[1].plot(ps,ps,'--',color='#999999',label='理想线性加速比')
    axs[1].set(xlabel='线程数量',ylabel='相对串行的加速比',title='实验二：线程数量与加速比');axs[1].legend(fontsize=8)
    for ax in axs:ax.set_xticks(ps);ax.grid(alpha=.2)
    for ext in ['png','pdf']:fig.savefig(OUT/f'实验二_线程性能.{ext}',dpi=200)
    plt.close(fig)
    # Visual comparison is supplementary; exact comparisons above are decisive.
    fig,axs=plt.subplots(1,3,figsize=(10,3.6),layout='constrained')
    for ax,name,title in zip(axs,['固定输入灰度图.png','serial.png','parallel_4.png'],['固定输入','串行输出','4线程输出']):
        ax.imshow(Image.open(OUT/name),cmap='gray',vmin=0,vmax=255);ax.set_title(title);ax.axis('off')
    fig.savefig(OUT/'输入与边缘结果对比.png',dpi=200);plt.close(fig)
    print(json.dumps(stats,indent=2));print('input SHA256:',digest)
if __name__=='__main__':main()
