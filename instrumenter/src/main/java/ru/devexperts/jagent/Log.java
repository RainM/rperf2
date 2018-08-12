//package com.devexperts.jagent;
//
///*
// * #%L
// * JAgent Impl
// * %%
// * Copyright (C) 2015 Devexperts, LLC
// * %%
// * This program is free software: you can redistribute it and/or modify
// * it under the terms of the GNU Lesser General Public License as
// * published by the Free Software Foundation, either version 3 of the
// * License, or (at your option) any later version.
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// * GNU General Lesser Public License for more details.
// *
// * You should have received a copy of the GNU General Lesser Public
// * License along with this program.  If not, see
// * <http://www.gnu.org/licenses/lgpl-3.0.html>.
// * #L%
// */
//
//        import java.io.FileNotFoundException;
//        import java.io.FileOutputStream;
//        import java.io.OutputStream;
//        import java.io.PrintWriter;
//
///**
// * Lightweight logger implementation.
// */
//public class Log {
//
//    private final Level logLevel;
//    private final String agentName;
//    private final LogPrintWriter out;
//
//    /**
//     * Creates logger.
//     *
//     * @param agentName agent name to be used in header
//     * @param level     logging level
//     * @param logFile   file to which log will be recorded, pass {@code null} to use standard output stream.
//     */
//    public Log(String agentName, Level level, String logFile) {
//        this.logLevel = level;
//        this.agentName = agentName;
//        LogPrintWriter tempOut = new LogPrintWriter(System.out);
//        if (logFile != null && !logFile.isEmpty()) {
//            try {
//                tempOut = new LogPrintWriter(new FileOutputStream(logFile));
//            } catch (FileNotFoundException | SecurityException e) {
//                tempOut.println("Failed to log to file: " + e);
//                e.printStackTrace(tempOut);
//            }
//        }
//        out = tempOut;
//    }
//
//    public Level getLogLevel() {
//        return logLevel;
//    }
//
//    public void log(Level level, Object msg) {
//        if (level.priority >= logLevel.priority)
//            out.println(msg);
//    }
//
//    public void log(Level level, Object msg1, Object msg2) {
//        if (level.priority >= logLevel.priority)
//            out.println("" + msg1 + msg2);
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3) {
//        if (level.priority >= logLevel.priority)
//            out.println("" + msg1 + msg2 + msg3);
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4) {
//        if (level.priority >= logLevel.priority)
//            out.println("" + msg1 + msg2 + msg3 + msg4);
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5) {
//        if (level.priority >= logLevel.priority)
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5);
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6) {
//        if (level.priority >= logLevel.priority)
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5 + msg6);
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7) {
//        if (level.priority >= logLevel.priority)
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5 + msg6 + msg7);
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8) {
//        if (level.priority >= logLevel.priority)
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5 + msg6 + msg7 + msg8);
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Object msg9) {
//        if (level.priority >= logLevel.priority)
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5 + msg6 + msg7 + msg8 + msg9);
//    }
//
//    public void log(Level level, Object... msgs) {
//        if (level.priority >= logLevel.priority) {
//            StringBuilder builder = new StringBuilder();
//            for (int i = 0; i < msgs.length; i++) {
//                if (i == msgs.length - 1 && msgs[i] instanceof Throwable) {
//                    Throwable t = (Throwable) msgs[i];
//                    t.printStackTrace();
//                    break;
//                }
//                builder.append(msgs[i]);
//            }
//            debug(builder);
//        }
//    }
//
//    public void log(Level level, Object msg, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println(msg);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println("" + msg1 + msg2);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println("" + msg1 + msg2 + msg3);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println("" + msg1 + msg2 + msg3 + msg4);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5 + msg6);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5 + msg6 + msg7);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5 + msg6 + msg7 + msg8);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void log(Level level, Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Object msg9, Throwable t) {
//        if (level.priority >= logLevel.priority) {
//            out.println("" + msg1 + msg2 + msg3 + msg4 + msg5 + msg6 + msg7 + msg8 + msg9);
//            t.printStackTrace(out);
//        }
//    }
//
//    public void debug(Object msg) {
//        log(Level.DEBUG, msg);
//    }
//
//    public void debug(Object... msgs) {
//        log(Level.DEBUG, msgs);
//    }
//
//    public void debug(Object msg1, Object msg2) {
//        log(Level.DEBUG, msg1, msg2);
//    }
//
//    public void debug(Object msg1, Object msg2, Object msg3) {
//        log(Level.DEBUG, msg1, msg2, msg3);
//    }
//
//    public void debug(Object msg1, Object msg2, Object msg3, Object msg4) {
//        log(Level.DEBUG, msg1, msg2, msg3, msg4);
//    }
//
//    public void debug(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5) {
//        log(Level.DEBUG, msg1, msg2, msg3, msg4, msg5);
//    }
//
//    public void debug(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6) {
//        log(Level.DEBUG, msg1, msg2, msg3, msg4, msg5, msg6);
//    }
//
//    public void debug(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7) {
//        log(Level.DEBUG, msg1, msg2, msg3, msg4, msg5, msg6, msg7);
//    }
//
//    public void debug(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8) {
//        log(Level.DEBUG, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8);
//    }
//
//    public void debug(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Object msg9) {
//        log(Level.DEBUG, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, msg9);
//    }
//
//    public void info(Object msg) {
//        log(Level.INFO, msg);
//    }
//
//    public void info(Object... msgs) {
//        log(Level.INFO, msgs);
//    }
//
//    public void info(Object msg1, Object msg2) {
//        log(Level.INFO, msg1, msg2);
//    }
//
//    public void info(Object msg1, Object msg2, Object msg3) {
//        log(Level.INFO, msg1, msg2, msg3);
//    }
//
//    public void info(Object msg1, Object msg2, Object msg3, Object msg4) {
//        log(Level.INFO, msg1, msg2, msg3, msg4);
//    }
//
//    public void info(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5) {
//        log(Level.INFO, msg1, msg2, msg3, msg4, msg5);
//    }
//
//    public void info(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6) {
//        log(Level.INFO, msg1, msg2, msg3, msg4, msg5, msg6);
//    }
//
//    public void info(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7) {
//        log(Level.INFO, msg1, msg2, msg3, msg4, msg5, msg6, msg7);
//    }
//
//    public void info(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8) {
//        log(Level.INFO, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8);
//    }
//
//    public void info(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Object msg9) {
//        log(Level.INFO, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, msg9);
//    }
//
//    public void warn(Object msg) {
//        log(Level.WARN, msg);
//    }
//
//    public void warn(Object... msgs) {
//        log(Level.WARN, msgs);
//    }
//
//    public void warn(Object msg1, Object msg2) {
//        log(Level.WARN, msg1, msg2);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3) {
//        log(Level.WARN, msg1, msg2, msg3);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4) {
//        log(Level.WARN, msg1, msg2, msg3, msg4);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, msg6);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, msg6, msg7);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Object msg9) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, msg9);
//    }
//
//    public void warn(Object msg, Throwable t) {
//        log(Level.WARN, msg, t);
//    }
//
//    public void warn(Object msg1, Object msg2, Throwable t) {
//        log(Level.WARN, msg1, msg2, t);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Throwable t) {
//        log(Level.WARN, msg1, msg2, msg3, t);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Throwable t) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, t);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Throwable t) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, t);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Throwable t) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, msg6, t);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Throwable t) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, msg6, msg7, t);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Throwable t) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, t);
//    }
//
//    public void warn(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Object msg9, Throwable t) {
//        log(Level.WARN, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, msg9, t);
//    }
//
//    public void error(Object msg) {
//        log(Level.ERROR, msg);
//    }
//
//    public void error(Object... msgs) {
//        log(Level.ERROR, msgs);
//    }
//
//    public void error(Object msg1, Object msg2) {
//        log(Level.ERROR, msg1, msg2);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3) {
//        log(Level.ERROR, msg1, msg2, msg3);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, msg6);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, msg6, msg7);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Object msg9) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, msg9);
//    }
//
//    public void error(Object msg, Throwable t) {
//        log(Level.ERROR, msg, t);
//    }
//
//    public void error(Object msg1, Object msg2, Throwable t) {
//        log(Level.ERROR, msg1, msg2, t);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Throwable t) {
//        log(Level.ERROR, msg1, msg2, msg3, t);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Throwable t) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, t);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Throwable t) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, t);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Throwable t) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, msg6, t);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Throwable t) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, msg6, msg7, t);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Throwable t) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, t);
//    }
//
//    public void error(Object msg1, Object msg2, Object msg3, Object msg4, Object msg5, Object msg6, Object msg7, Object msg8, Object msg9, Throwable t) {
//        log(Level.ERROR, msg1, msg2, msg3, msg4, msg5, msg6, msg7, msg8, msg9, t);
//    }
//
//    public enum Level {
//        DEBUG(1), INFO(2), WARN(3), ERROR(4);
//
//        private int priority;
//
//        Level(int priority) {
//            this.priority = priority;
//        }
//    }
//
////    private class LogPrintWriter extends PrintWriter {
////
////        private final OutputStream out;
////
////        public LogPrintWriter(OutputStream out) {
////            super(new FastOutputStreamWriter(out), true);
////            this.out = out;
////        }
////
////        @Override
////        public void println(Object x) {
////            if (x instanceof CharSequence)
////                println((CharSequence) x);
////            else
////                println(String.valueOf(x));
////        }
////
////        @Override
////        public void println(String x) {
////            synchronized (out) {
////                printHeader();
////                super.println(x);
////            }
////        }
////
////        private void println(CharSequence cs) {
////            synchronized (out) {
////                printHeader();
////                for (int i = 0; i < cs.length(); i++)
////                    super.print(cs.charAt(i));
////                super.println();
////            }
////        }
////
////        private void printHeader() {
////            FastFmtUtil.printTimeAndDate(this, System.currentTimeMillis());
////            print(' ');
////            print(agentName);
////            print(':');
////            print(' ');
////        }
////    }
//}