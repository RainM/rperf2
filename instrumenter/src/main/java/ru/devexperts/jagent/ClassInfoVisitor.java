package com.devexperts.jagent;

/*
 * #%L
 * JAgent Impl
 * %%
 * Copyright (C) 2015 - 2016 Devexperts, LLC
 * %%
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Lesser Public License for more details.
 *
 * You should have received a copy of the GNU General Lesser Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/lgpl-3.0.html>.
 * #L%
 */

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.Opcodes;

public class ClassInfoVisitor extends ClassVisitor {
    private ClassInfo.Builder classInfoBuilder = new ClassInfo.Builder();

    public ClassInfoVisitor() {
        super(Opcodes.ASM5);
    }

    @Override
    public void visit(int version, int access, String name, String signature, String superName, String[] interfaces) {
        classInfoBuilder.version(version).access(access).internalName(name).internalSuperName(superName).internalInterfaceNames(interfaces);
    }

    @Override
    public void visitSource(String source, String debug) {
        classInfoBuilder.sourceFile(source);
    }

    public ClassInfo buildClassInfo() {
        return classInfoBuilder.build();
    }
}