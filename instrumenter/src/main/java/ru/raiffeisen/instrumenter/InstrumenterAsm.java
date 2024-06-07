package ru.raiffeisen.instrumenter;

import ru.devexperts.jagent.ClassInfoCache;
import ru.devexperts.jagent.ClassInfoVisitor;
import ru.devexperts.jagent.FrameClassWriter;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import ru.raiffeisen.PerfPtProf;

import java.io.File;
import java.io.IOException;
import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.IllegalClassFormatException;
import java.lang.instrument.Instrumentation;
import java.nio.file.Files;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static org.objectweb.asm.Opcodes.ASM5;
import static org.objectweb.asm.Opcodes.V1_8;

/**
 * Created by sergey on 12.03.17.
 */
public class InstrumenterAsm {

    //private static ClassLoader agentClassLoader = new InnerJarClassLoader();
    private static ClassInfoCache cache = new ClassInfoCache();

    public static void premain(String arguments, Instrumentation instr) {
        ArrayList<Pattern> patterns_local = new ArrayList<>();
        if (arguments != null && !arguments.isEmpty()) {
            String[] args = arguments.split(",");
            Arrays.stream(args).forEach((s) -> s.replace(".", "/"));
            //patterns_local = Arrays.stream(args).map(Pattern::compile).toArray(Pattern[]::new);
            for (String str : args) {
                Pattern p = Pattern.compile(str);
                patterns_local.add(p);
            }
        }
        Pattern[] patterns = patterns_local.toArray(new Pattern[patterns_local.size()]);

        String triggerMethod = System.getProperty("TRIGGER_METHOD");
        String triggerClass = System.getProperty("TRIGGER_CLASS");
        String triggerMethodSignature = System.getProperty("TRIGGER_METHOD_SIGNATURE");
        String countdownStr = System.getProperty("TRIGGER_COUNTDOWN");
        String percentile_str = System.getProperty("PERCENTILE");
	String perf_buf_sz_str = System.getProperty("PERF_BUF_SZ");
	String perf_aux_sz_str = System.getProperty("PERF_AUX_SZ");
	String trace_max_sz_str = System.getProperty("TRACE_MAX_SZ");
	String trace_dest = System.getProperty("TRACE_DEST");

	long perf_buf_sz = perf_buf_sz_str != null ? Long.valueOf(perf_buf_sz_str) : 32;
	long perf_aux_sz = perf_aux_sz_str != null ? Long.valueOf(perf_aux_sz_str) : 256;
	long trace_max_sz = trace_max_sz_str != null ? Long.valueOf(trace_max_sz_str) : 10_000_000;
	double percentile = percentile_str != null ? Double.valueOf(percentile_str) : 0;

        int countdown = countdownStr != null ? Integer.valueOf(countdownStr) : 1;
        PerfPtProf.init(countdown, percentile, perf_buf_sz, perf_aux_sz, trace_max_sz, trace_dest);

        System.out.println("Trigger class: " + triggerClass);
        System.out.println("Trigger method: " + triggerMethod);
        System.out.println("Trigger method signature: " + triggerMethodSignature);

        instr.addTransformer(new ClassFileTransformer() {
            public
            byte[]
            transform(  ClassLoader         loader,
                        String              className,
                        Class<?>            classBeingRedefined,
                        ProtectionDomain protectionDomain,
                        byte[]              classfileBuffer)
                    throws IllegalClassFormatException {
                boolean should_transform = patterns.length == 0;

                for (Pattern p : patterns) {
                    Matcher m = p.matcher(className);
                    if (m.find()) {
                        should_transform = true;
                        break;
                    }
                }

                if (!should_transform) {
                    return null;
                }

                ClassReader cr = new ClassReader(classfileBuffer);

                if ((cr.getAccess() & Opcodes.ACC_INTERFACE) != 0) {
                    return classfileBuffer;
                }

                ClassInfoVisitor ciVisitor = new ClassInfoVisitor();
                cr.accept(ciVisitor, 0);
                // update cache with information about classes
                cache.getOrInitClassInfoMap(loader).put(className, ciVisitor.buildClassInfo());
                // create FrameClassWriter using cache

                ClassWriter cw = //new ClassWriter(COMPUTE_MAXS | COMPUTE_FRAMES);
                        new FrameClassWriter(loader, cache, V1_8);

                cr.accept(new ClassInstrumenter(ASM5, cw, triggerClass, triggerMethod, triggerMethodSignature), 0);

                return cw.toByteArray();
            }
        });

    }

    private static class MethodAdapter extends MethodVisitor {

        public MethodAdapter(int i, MethodVisitor methodVisitor) {
            super(i, methodVisitor);
        }

        @Override
        public void visitCode() {
            mv.visitIntInsn(Opcodes.BIPUSH, 123);
        }
    }
}
