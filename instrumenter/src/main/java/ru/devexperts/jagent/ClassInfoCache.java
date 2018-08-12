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

import org.objectweb.asm.ClassReader;

import java.io.InputStream;
import java.util.*;

public class ClassInfoCache {

    //private final Log log;

    // ClassLoader -> internalClassName -> ClassInfo
    private WeakHashMap<ClassLoader, ClassInfoMap> classInfoCache = new WeakHashMap<>();

    public ClassInfoCache(/*Log log*/) {
        //this.log = log;
    }

    public synchronized ClassInfo getClassInfo(String internalClassName, ClassLoader loader) {
        while (true) {
            ClassInfo classInfo = getOrInitClassInfoMap(loader).get(internalClassName);
            if (classInfo != null)
                return classInfo;
            if (loader == null)
                return null;
            loader = loader.getParent();
        }
    }

    // Returns null if not found or failed to load
    public synchronized ClassInfo getOrBuildClassInfo(String internalClassName, ClassLoader loader) {
        ClassInfo classInfo = getOrInitClassInfoMap(loader).get(internalClassName);
        if (classInfo != null)
            return classInfo;
        ClassInfoMap classInfoMap = classInfoCache.get(loader);
        classInfo = buildClassInfo(internalClassName, loader);
        if (classInfo != null)
            classInfoMap.put(internalClassName, classInfo);
        return classInfo;
    }

    // throws RuntimeException if not found or failed to load
    synchronized ClassInfo getOrBuildRequiredClassInfo(String internalClassName, ClassLoader loader) {
        ClassInfo classInfo = getOrBuildClassInfo(internalClassName, loader);
        if (classInfo == null)
            throw new RuntimeException("Cannot load class information for " + internalClassName.replace('/', '.'));
        return classInfo;
    }

    public synchronized ClassInfoMap getOrInitClassInfoMap(ClassLoader loader) {
        ClassInfoMap classInfoMap = classInfoCache.get(loader);
        if (classInfoMap == null) {
            // make sure we have parent loader's map first
            if (loader != null)
                getOrInitClassInfoMap(loader.getParent());
            // at first time when class loader is discovered, tracked classes in this class loader are cached
            classInfoCache.put(loader, classInfoMap = new ClassInfoMap());
        }
        return classInfoMap;
    }

    private ClassInfo buildClassInfo(String internalClassName, ClassLoader loader) {
        // check if parent class loader has this class info
        if (loader != null)  {
            ClassInfo classInfo = getOrBuildClassInfo(internalClassName, loader.getParent());
            if (classInfo != null)
                return classInfo;
        }
        // actually build it
        try {
            String classFileName = internalClassName + ".class";
            InputStream in;
            if (loader == null)
                in = getClass().getResourceAsStream("/" + classFileName);
            else
                in = loader.getResourceAsStream(classFileName);
            if (in == null)
                return null;
            ClassInfoVisitor visitor = new ClassInfoVisitor();
            try {
                ClassReader cr = new ClassReader(in);
                cr.accept(visitor, ClassReader.SKIP_DEBUG + ClassReader.SKIP_FRAMES + ClassReader.SKIP_CODE);
            } finally {
                in.close();
            }
            return visitor.buildClassInfo();
        } catch (Throwable t) {
            throw new RuntimeException(t);
            //log.error("Failed to build class info for ", internalClassName, " loaded by ", loader, t);
            //return null;
        }
    }

}