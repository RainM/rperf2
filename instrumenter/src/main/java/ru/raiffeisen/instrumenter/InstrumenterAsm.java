package ru.raiffeisen.instrumenter;

import com.devexperts.jagent.ClassInfoCache;
import com.devexperts.jagent.ClassInfoVisitor;
import com.devexperts.jagent.FrameClassWriter;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import ru.raiffeisen.PerfPtProf;

import java.io.File;
import java.io.IOException;
import java.lang.instrument.Instrumentation;
import java.nio.file.Files;
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
        Boolean dump_class_files = System.getProperty("DUMP_OUT_CLASSES") != null;

        String triggerMethod = System.getProperty("TRIGGER_METHOD");
        String triggerClass = System.getProperty("TRIGGER_CLASS");
        String triggerMethodSignature = System.getProperty("TRIGGER_METHOD_SIGNATURE");
        String countdownStr = System.getProperty("TRIGGER_COUNTDOWN");
        String percentile = System.getProperty("PERCENTILE");

        int countdown = countdownStr != null ? Integer.valueOf(countdownStr) : 1;
        PerfPtProf.init(countdown, Double.valueOf(percentile));

        System.out.println("Trigger class: " + triggerClass);
        System.out.println("Trigger method: " + triggerMethod);
        System.out.println("Trigger method signature: " + triggerMethodSignature);

        instr.addTransformer((classLoader, s, aClass, protectionDomain, bytes) -> {

            boolean should_transform = patterns.length == 0;

            for (Pattern p : patterns) {
                Matcher m = p.matcher(s);
                if (m.find()) {
                    should_transform = true;
                    break;
                }
            }

            if (!should_transform) {
                return null;
            }

            ClassReader cr = new ClassReader(bytes);

            if ((cr.getAccess() & Opcodes.ACC_INTERFACE) != 0) {
                return bytes;
            }

            ClassInfoVisitor ciVisitor = new ClassInfoVisitor();
            cr.accept(ciVisitor, 0);
            // update cache with information about classes
            cache.getOrInitClassInfoMap(classLoader).put(s, ciVisitor.buildClassInfo());
            // create FrameClassWriter using cache

            ClassWriter cw = //new ClassWriter(COMPUTE_MAXS | COMPUTE_FRAMES);
                    new FrameClassWriter(classLoader, cache, V1_8);

            cr.accept(new ClassInstrumenter(ASM5, cw, triggerClass, triggerMethod, triggerMethodSignature), 0);

            byte[] result = cw.toByteArray();

            if (dump_class_files) {
                File out_dir = new File("out");
                out_dir.mkdir();

                File out_file = new File("out/" + s + ".class");
                new File(out_file.getParent()).mkdirs();

                try {
                    Files.write(out_file.toPath(), result);
                } catch (IOException e) {
                    e.printStackTrace();
                    System.exit(2);
                }
            }

            return result;
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
