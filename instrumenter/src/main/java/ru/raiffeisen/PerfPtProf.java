package ru.raiffeisen;

public class PerfPtProf {
    public static native void init(int skip_n, double percentile, long buf_sz, long aux_sz, long trace_sz, String print_trace);
    public static native void start();
    public static native void stop();
}
