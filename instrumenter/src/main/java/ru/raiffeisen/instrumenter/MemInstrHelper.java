package ru.raiffeisen.instrumenter;

import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;

/**
 * Created by sergey on 16.04.17.
 */
public enum MemInstrHelper {
    VOID        (Type.VOID,     -1,                -1,             Opcodes.RETURN,      0),
    BOOLEAN     (Type.BOOLEAN,  Opcodes.ILOAD,     Opcodes.ISTORE, Opcodes.IRETURN,     1),
    BYTE        (Type.BYTE,     Opcodes.ILOAD,     Opcodes.ISTORE, Opcodes.IRETURN,     1),
    CHAR        (Type.CHAR,     Opcodes.ILOAD,     Opcodes.ISTORE, Opcodes.IRETURN,     1),
    SHORT       (Type.SHORT,    Opcodes.ILOAD,     Opcodes.ISTORE, Opcodes.IRETURN,     1),
    INT         (Type.INT,      Opcodes.ILOAD,     Opcodes.ISTORE, Opcodes.IRETURN,     1),
    FLOAT       (Type.FLOAT,    Opcodes.FLOAD,     Opcodes.FSTORE, Opcodes.FRETURN,     1),
    REF         (Type.OBJECT,   Opcodes.ALOAD,     Opcodes.ASTORE, Opcodes.ARETURN,     1),
    ARRAY       (Type.ARRAY,    Opcodes.ALOAD,     Opcodes.ASTORE, Opcodes.ARETURN,     1),
    LONG        (Type.LONG,     Opcodes.LLOAD,     Opcodes.LSTORE, Opcodes.LRETURN,     2),
    DOUBLE      (Type.DOUBLE,   Opcodes.DLOAD,     Opcodes.DSTORE, Opcodes.DRETURN,     2);

    private static MemInstrHelper[] mem_instr_by_type = new MemInstrHelper[12];

    static {
        for (MemInstrHelper instr : MemInstrHelper.values()) {
            mem_instr_by_type[instr.type] = instr;
        }
    }

    public final int type;
    public final int load_instr;
    public final int store_instr;
    public final int ret_instr;
    public final int op_size;

    private MemInstrHelper(int type, int load_instr, int store_instr, int ret_instr, int op_size) {
        this.type = type;
        this.load_instr = load_instr;
        this.store_instr = store_instr;
        this.ret_instr = ret_instr;
        this.op_size = op_size;
    }

    public static MemInstrHelper instr_helper_by_op_type(int type) {
        return mem_instr_by_type[type];
    }
}
