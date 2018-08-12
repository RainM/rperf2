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

import org.objectweb.asm.ClassWriter;

import static org.objectweb.asm.Opcodes.V1_6;

public class FrameClassWriter extends ClassWriter {
    private static final String OBJECT = "java/lang/Object";

    private final ClassLoader loader; // internalClassName -> ClassInfo
    private final ClassInfoCache ciCache;

    public FrameClassWriter(ClassLoader loader, ClassInfoCache ciCache, int classVersion) {
        super(classVersion > V1_6 ? COMPUTE_FRAMES : COMPUTE_MAXS);
        this.loader = loader;
        this.ciCache = ciCache;
    }

    /**
     * The reason of overriding is to avoid ClassCircularityError (or LinkageError with "duplicate class loading" reason)
     * which occurs during processing of classes related to java.util.TimeZone and use cache of ClassInfo.
     */
    @Override
    protected String getCommonSuperClass(String type1, String type2) {
        ClassInfo c = ciCache.getOrBuildRequiredClassInfo(type1, loader);
        ClassInfo d = ciCache.getOrBuildRequiredClassInfo(type2, loader);

        if (c.isAssignableFrom(d, ciCache, loader))
            return type1;
        if (d.isAssignableFrom(c, ciCache, loader))
            return type2;

        if (c.isInterface() || d.isInterface()) {
            return OBJECT;
        } else {
            do {
                c = c.getSuperclassInfo(ciCache, loader);
            } while (c != null && !c.isAssignableFrom(d, ciCache, loader));

            return c == null ? OBJECT : c.getInternalName();
        }
    }
}