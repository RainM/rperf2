package ru.raiffeisen;

import com.google.gson.Gson;
import com.google.gson.JsonParseException;
import com.google.gson.annotations.Expose;
import com.google.gson.reflect.TypeToken;

import java.io.StringWriter;
import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.ArrayList;
import java.util.TreeSet;

/**
 * JSON tester app 1
 *
 */
public class App2
{
    
    private static Gson gson = new Gson();


    private interface IFace {
	int foo(int arg);
    }

    public static class Impl1 implements IFace {
	public int foo(int arg) {
	    return arg+1;
	}
    }

    public static class Impl2 implements IFace {
	public int foo(int arg) {
	    return arg - 1;
	}
    }

    public static String generate_json(int count) {
        StringBuilder sb = new StringBuilder();
        boolean first = true;
        sb.append('{');
        for (int i = 0; i < count; ++i) {
            if (first) {
                first = false;
            } else {
                sb.append(',');
            }
            sb.append("name").append(i).append(": 'value").append(i).append("'");
        }
        sb.append('}');
        return sb.toString();
    }

    public static class MyPair {
	public Map map_value;
	public int int_value;
    }

    public static Map decode(String json, int idx) {
	Map result = gson.fromJson(json, Map.class);
	return result;
    }

    public static Map decode_1(String json, int idx) {
	if (idx > 17000 && idx < 17010) {
	    json = "{\"qqq\": \"123\"}";
	}
	return decode(json, idx);
    }

    public static void main( String[] args )
    {
	int r = 0;
	String json = generate_json(3000);
	for (int i = 0; i < 2000000; ++i) {
	    Map m = decode_1(json, i);

	    if (i % 1000 == 0) {
		System.out.println("i = " + i);
	    }

	    r += m.size();
	}
	System.out.println("r = " + r);
    }


    /*
    public static int test(IFace f) {
	int result = 0;
	for (int i =0; i < 1000000; ++i) {
	    result += f.foo(i);
	}
	return result;
    }

    public static void main( String[] args )
    {
	int r = 0;
	for (int i = 0; i < 2000000; ++i) {
	    if (i == 17000) {
		//r += test(new Impl2());
		r -= 1;
	    }
	    r += test(new Impl1());
	    
	    if (i % 1000 == 0) {
		System.out.println("i = " + i);
	    }
	}
	System.out.println("r = " + r);
    }
    */


}
