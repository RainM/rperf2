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

/**
 * JSON tester app 1
 *
 */
public class App 
{
    private static Gson gson = new Gson();

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

    public static Map decode(String json) {
	Map result = gson.fromJson(json, Map.class);
	return result;
    }

    public static void main( String[] args )
    {
        System.out.println( "Hello World!" );
	
	for (int i = 0; i < 20000; ++i) {
	    String json = generate_json(i);
	    Map res = decode(json);
	    
	    if (res.size() != i) {
		System.out.println("Something went wrong...");
	    }
	    
	    if (i % 1000 == 0) {
		System.out.println("i = " + i);
	    }
	}
    }
}
