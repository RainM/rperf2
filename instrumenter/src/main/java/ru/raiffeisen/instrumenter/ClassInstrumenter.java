package ru.raiffeisen.instrumenter;

import org.objectweb.asm.*;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;

/**
 * Created by Sergey Melnikov on 16.04.17.
 */
class ClassInstrumenter extends ClassVisitor {

    private final static boolean OUTPUT_PROCESSED_CLASSES = System.getProperty("OUTPUT_INSTRUMENTED_CLASSES") != null;
    private final String triggerClass;
    private final String triggerMethod;
    private final String triggerMethodSignature;
    private List<MethodToGenerateInfo> methods_to_generate = new ArrayList<>();
    private String class_name;

    public ClassInstrumenter(
            int i,
            ClassVisitor classVisitor,
            String triggerClass_,
            String triggerMethod_,
            String triggerMethodSignature_) {
        super(i, classVisitor);
        triggerClass = triggerClass_;
        triggerMethod = triggerMethod_;
        triggerMethodSignature = triggerMethodSignature_;
    }

    public void visit(int version, int access, String name, String signature,
                      String superName, String[] interfaces) {
        class_name = name;
        super.visit(version, access, name, signature, superName, interfaces);
    }

    @Override
    public MethodVisitor visitMethod(int access, String name, String desc, String signature, String[] exceptions) {
        if ((access & Opcodes.ACC_ABSTRACT) == 0) {
            if (!name.equals("<init>")) {
                if (!name.equals("<clinit>")) {
                    if (class_name.equals(triggerClass)) {
                        if (name.equals(triggerMethod)) {
                            if (OUTPUT_PROCESSED_CLASSES) {
                                System.out.println("Processing: " + class_name + "::" + name + "/" + signature);
                                String wrapped_name = "__$$" + class_name + "$$" + name + "$$IMPL$$__";

                                MethodVisitor mv;

                                mv = cv.visitMethod(access, wrapped_name, desc, signature, exceptions);

                                methods_to_generate.add(new MethodToGenerateInfo(access,
                                        name,
                                        desc,
                                        signature,
                                        exceptions,
                                        wrapped_name,
                                        class_name));

                                return mv;

                            }
                        }
                    }

                }
            }
        }
        MethodVisitor mv = cv.visitMethod(access, name, desc, signature, exceptions);
        return mv;
    }

    @Override
    public void visitEnd() {
        for (MethodToGenerateInfo info : methods_to_generate) {
            MethodVisitor stub_generator = cv.visitMethod(info.access, info.name, info.desc, info.signature, info.exceptions);
            generate_method_to_profile(stub_generator, info);
        }

        super.visitEnd();
    }

    private void generate_method_to_profile(MethodVisitor mv, MethodToGenerateInfo info) {

        Type[] signature = Type.getArgumentTypes(info.desc);

        int local_head_top = 0;

        local_head_top = emit_tracer_start(mv, info, local_head_top);

        Label try_begin = new Label();
        mv.visitLabel(try_begin);

        local_head_top = emit_impl_call(mv, info, signature, local_head_top);


        Label try_end = new Label();
        mv.visitLabel(try_end);

        emit_tracer_end(mv, "stop");
        local_head_top = emit_normal_return(mv, info, local_head_top);


        Label finally_begin = new Label();
        mv.visitLabel(finally_begin);

        local_head_top = emit_exception_handler(mv, local_head_top, (heap_top) -> {
            int top = heap_top;
            emit_tracer_end(mv, "stop");
            return top;
        });

        mv.visitTryCatchBlock(try_begin, try_end, finally_begin, null);

        mv.visitMaxs(local_head_top + 1, local_head_top + 1);
        mv.visitEnd();
    }

    private int emit_exception_handler(MethodVisitor mv, int local_heap_top, Function<Integer, Integer> handler_body) {
        int exception_idx = ++local_heap_top;
        mv.visitVarInsn(Opcodes.ASTORE, exception_idx);

        local_heap_top = handler_body.apply(local_heap_top);

        mv.visitVarInsn(Opcodes.ALOAD, exception_idx);
        mv.visitInsn(Opcodes.ATHROW);
        return local_heap_top;
    }

    private int emit_normal_return(MethodVisitor mv, MethodToGenerateInfo info, int local_heap_top) {
        Type ret_type = Type.getReturnType(info.desc);

        int store_opcode;
        int load_opcode;
        int ret_opcode;
        boolean is_void;

        MemInstrHelper instr_helper = MemInstrHelper.instr_helper_by_op_type(ret_type.getSort());

        store_opcode = instr_helper.store_instr;
        load_opcode = instr_helper.load_instr;
        ret_opcode = instr_helper.ret_instr;
        is_void = instr_helper == MemInstrHelper.VOID;

        if (!is_void) {
            int ret_value_idx = ++local_heap_top;
            mv.visitVarInsn(store_opcode, ret_value_idx);
            mv.visitVarInsn(load_opcode, ret_value_idx);
            mv.visitInsn(ret_opcode);
        } else {
            mv.visitInsn(Opcodes.RETURN);
        }
        return local_heap_top;
    }

    private void emit_tracer_end(MethodVisitor mv,
                                 String method_name) {
        mv.visitMethodInsn(Opcodes.INVOKESTATIC, "ru/raiffeisen/PerfPtProf", method_name, "()V", false);
    }

    private int emit_impl_call(MethodVisitor mv, MethodToGenerateInfo info, Type[] signature, int local_head_top) {
        for (Type t : signature) {
            MemInstrHelper instr_type = MemInstrHelper.instr_helper_by_op_type(t.getSort());

            mv.visitVarInsn(instr_type.load_instr, local_head_top);

            local_head_top += instr_type.op_size;

        }

        int call_opcode = Opcodes.INVOKEVIRTUAL;
        if ((info.access & Opcodes.ACC_STATIC) != 0) {
            call_opcode = Opcodes.INVOKESTATIC;
        }

        mv.visitMethodInsn(call_opcode, class_name, info.method_to_call, info.desc, false);
        return local_head_top;
    }

    private int emit_tracer_start(MethodVisitor mv, MethodToGenerateInfo info, int idx) {
        emit_tracer_end(mv, "start");

        if ((info.access & Opcodes.ACC_STATIC) == 0) {
            mv.visitVarInsn(Opcodes.ALOAD, 0);
            idx += 1;
        }
        return idx;
    }

    private static class MethodToGenerateInfo {
        public final int access;
        public final String name;
        public final String desc;
        public final String signature;
        public final String[] exceptions;
        public final String method_to_call;
        public final String clazz_name;

        private MethodToGenerateInfo(int access,
                                     String name,
                                     String desc,
                                     String signature,
                                     String[] exceptions,
                                     String method_to_call,
                                     String clazz_name) {
            this.access = access;
            this.name = name;
            this.desc = desc;
            this.signature = signature;
            this.exceptions = exceptions;
            this.method_to_call = method_to_call;
            this.clazz_name = clazz_name;
        }
    }
}
