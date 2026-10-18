package com.v7878.vmtools;

import static com.v7878.unsafe.AndroidUnsafe.PAGE_SIZE;
import static com.v7878.unsafe.Utils.shouldNotHappen;
import static com.v7878.unsafe.misc.Math.roundDownUL;
import static com.v7878.unsafe.misc.Math.roundUpUL;

import android.system.ErrnoException;

import com.v7878.unsafe.io.IOUtils;

import java.lang.invoke.MethodType;
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.List;

final class _Utils {
    public static MethodType rawMethodTypeOf(Executable ex) {
        Class<?> ret;
        List<Class<?>> args = new ArrayList<>();
        if (ex instanceof Method m) {
            ret = m.getReturnType();
            if (!Modifier.isStatic(m.getModifiers())) {
                args.add(m.getDeclaringClass());
            }
            args.addAll(List.of(m.getParameterTypes()));
        } else {
            assert ex instanceof Constructor<?>;
            ret = void.class;
            args.add(ex.getDeclaringClass());
            args.addAll(List.of(ex.getParameterTypes()));
        }
        return MethodType.methodType(ret, args);
    }

    public static void aligned_mprotect(long address, long length, int prot) {
        long end = roundUpUL(address + length, PAGE_SIZE);
        long begin = roundDownUL(address, PAGE_SIZE);
        try {
            IOUtils.mprotect(begin, end - begin, prot);
        } catch (ErrnoException e) {
            throw shouldNotHappen(e);
        }
    }
}
