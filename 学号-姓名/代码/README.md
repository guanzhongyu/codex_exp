# 基于 OpenMP 的 Sobel 边缘检测

C++17 自行实现 Sobel 核心；PGM 读写不依赖 OpenCV。主实验只改变实现方式和线程数，固定同一张 4096×4096 灰度图。

## 文件与构建

需要 Linux、G++（支持 OpenMP）和 Make。绘图需要 Python 3 及 requirements.txt 中的包。
在本目录执行：

```bash
make -j4 all
make test
make sanitize
```

二进制输出至仓库根目录 `temp/bin/`。`serial.cpp` 与 `parallel.cpp` 均可独立编译为命令行程序，`sobel.hpp` 复用单行计算以保持输出规则一致，`io.cpp` 实现 P5 PGM 读写。`verify.cpp` 用独立3×3参考卷积与手算样例检验正确性。`benchmark.cpp` 在同一进程中完成交错计时。

单独运行（只接受8位二进制 P5 PGM，最大灰度255）：

```bash
../../temp/bin/serial ../实验结果/固定输入灰度图.pgm ../../temp/serial_check.pgm
../../temp/bin/parallel ../实验结果/固定输入灰度图.pgm ../../temp/parallel_check.pgm 4
cmp ../../temp/serial_check.pgm ../../temp/parallel_check.pgm
```

命令行程序显示单次核心时间，包含该次OpenMP初始化开销；报告使用以下预热后的基准数据，不使用这些单次时间。

## 复现实验

已有依赖时直接运行；缺少依赖可在仓库根目录temp下创建虚拟环境：

```bash
python3 -m venv ../../temp/venv
../../temp/venv/bin/python -m pip install -r requirements.txt
../../temp/venv/bin/python run_experiment.py
```

当前云环境已有绘图依赖，执行 `python3 run_experiment.py` 即可。脚本会编译并运行普通测试及ASan/UBSan测试，生成固定输入、检查线程团队大小，进行3次预热及每组30次计时，输出CSV、PNG/PDF图表、环境元数据和验证日志。

**重新运行会覆盖实验结果。** 新硬件测量会变化，应重新生成报告，不能把新CSV与旧报告混用。报告生成需要python-docx和LibreOffice；生成器位于 `../../temp/build_report.py`，当前以 `/workspace/codex_exp` 为根目录。先执行生成器，再用 `soffice --headless --convert-to pdf` 转换生成的DOCX。

## 算法与正确性

- 水平核 `[-1,0,1;-2,0,2;-1,0,1]`，垂直核 `[-1,-2,-1;0,0,0;1,2,1]`。
- 内部像素输出 `floor(min(255,sqrt(Gx²+Gy²)))`；最外圈输出0，非输入零填充卷积。
- 每次调用覆盖全部输出；输入和输出缓冲区必须分离。
- 并行按行使用 `parallel for schedule(static)`，输入只读，各线程写入互不重叠的行。
- 7种尺寸×5类输入×4种实现配置共140次参考对照，另有中心输出24的手算检查。
- 正式计时前、每次正式计时后都验证结果；120个计时输出均与串行逐像素一致。

## 实验口径

只开展两项性能实验：串行对4线程；OpenMP 1/2/4线程扩展。容器可见5个逻辑CPU，但CPU配额等效4核，因此不将8线程列为主实验配置。

各组使用同一编译选项 `-O3 -std=c++17 -fopenmp -Wall -Wextra -Wpedantic`。`OMP_DYNAMIC=FALSE`，`OMP_PROC_BIND=close`，`OMP_PLACES=cores`，`OMP_WAIT_POLICY=PASSIVE`。每轮随机排列四种配置，固定随机种子20261025，全部样本保留。只计核心运算、边界写入及并行运行时成本，排除I/O、分配、比较、绘图。

汇总取算术均值、样本标准差；加速比=串行均值/该组均值，效率=加速比/线程数。图中误差线为标准差。输入是自生成合成图而非自然图像，所有实验共用一份PGM，SHA-256见元数据。参考文献中的OpenCV只用于核对Sobel公式，代码没有调用其算子。

## 本次结果（2026-10-25）

| 配置 | 平均时间/ms | 标准差/ms | 相对串行加速比 | 并行效率 |
|---|---:|---:|---:|---:|
| 串行 |65.850|10.342|1.000|—|
| OpenMP 1线程 |64.771|7.304|1.017|101.67%|
| OpenMP 2线程 |37.097|7.501|1.775|88.75%|
| OpenMP 4线程 |34.074|6.033|1.933|48.31%|

4线程核心时间减少48.25%；2到4线程仅再减少8.15%。云环境有波动；1线程略快于串行不构成稳定收益的证据。基准前后配额节流计数未增加；未使用硬件计数器测量各项成本，不能确认收益递减的单一原因，也未测量超过4线程的性能。

原始120条数据在 `../实验结果/原始计时数据.csv`，汇总表在 `性能测试数据.csv`。报告含完整分析。代码和报告包含AI辅助生成内容，提交者需据实署名，并自行理解和复核。
