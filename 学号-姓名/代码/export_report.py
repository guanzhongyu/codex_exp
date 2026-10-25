#!/usr/bin/env python3
"""生成DOCX、导出PDF，并清空作者及编辑者元数据。"""
from pathlib import Path
import subprocess
import sys
import fitz

code = Path(__file__).resolve().parent
report = code.parent / '报告'
subprocess.run([sys.executable, str(code/'build_report.py')], check=True)
subprocess.run(['soffice', '-env:UserInstallation=file:///tmp/sobel_report_profile',
                '--headless', '--convert-to', 'pdf', '--outdir', str(report),
                str(report/'程序设计报告.docx')], check=True)
path = report/'程序设计报告.pdf'
with fitz.open(path) as doc:
    metadata = doc.metadata
    for field in ['author', 'creator', 'producer', 'keywords']:
        metadata[field] = ''
    doc.set_metadata(metadata)
    doc.del_xml_metadata()
    clean = path.with_name('report_clean.pdf')
    doc.save(clean, garbage=4, deflate=True)
clean.replace(path)
print(path)
