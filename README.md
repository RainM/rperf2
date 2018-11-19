# rperf2
Tool for latency-analysis in Java

## How it works

There are two modes for rperf2:

1) Classic mode: just skip N invocaitons and profile N+1 invocation
2) Quantile analysis mode: you specify required percentile, after that profiler will wait for appropriate case of slowing down and outputs TOP and trace

## How to build

1. Download with modules
```git clone --recursive REPO_URL_YOU_USE```
2. Build profiler
```make build-libipt``` (may require some environment variables to be set)
3. Build instrumenter agent
```make build-instrumenter```
4. Use it! See examples at Makefile
