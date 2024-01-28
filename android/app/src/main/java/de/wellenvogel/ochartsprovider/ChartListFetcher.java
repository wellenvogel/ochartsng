package de.wellenvogel.ochartsprovider;


import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

public class ChartListFetcher  {
    public interface ResultHandler{
        void storeResult(JSONArray o, String error);
    }
    private String url;
    long sequence=0;
    int timeoutMs=0;
    Thread thread=null;
    ResultHandler handler=null;
    public ChartListFetcher(ResultHandler handler,String url, int timeoutMs){
        this.url=url;
        this.timeoutMs=timeoutMs;
        this.handler=handler;
    }
    private synchronized void queryDone(long sequence){
        if (this.sequence == sequence) thread=null;
    }
    private synchronized void storeResult(long sequence,JSONArray r,String error){
        if (sequence != this.sequence) return;
        handler.storeResult(r,error);
    }
    public synchronized void query(){
        sequence++;
        if (thread != null){
            thread.interrupt();
        }
        final long s=sequence;
        thread = new Thread(new Runnable() {
            @Override
            public void run() {
                try{
                    URL fetchUrl=new URL(url);
                    HttpURLConnection connection=(HttpURLConnection)fetchUrl.openConnection();
                    connection.setConnectTimeout(timeoutMs);
                    connection.setReadTimeout(timeoutMs);
                    int r=connection.getResponseCode();
                    if (r != HttpURLConnection.HTTP_OK){
                        throw new Exception(("HTTP error "+r));
                    }
                    StringBuffer data=new StringBuffer();
                    BufferedReader rd=new BufferedReader(new InputStreamReader(connection.getInputStream()));
                    String line;
                    while((line = rd.readLine()) != null){
                        data.append(line);
                    }
                    JSONObject rt=new JSONObject(data.toString());
                    if (!rt.has("status") || ! "OK".equals(rt.getString("status"))){
                        if (rt.has("info")){
                            storeResult(s,null,rt.getString("info"));
                        }
                        else{
                            storeResult(s,null,"invalid status in response");
                        }
                    }
                    else{
                        if (! rt.has("data")){
                            storeResult(s,null,"missing data in response");
                        }
                        else{
                            storeResult(s,rt.getJSONArray("data"),null);
                        }
                    }
                }catch (Throwable t){
                    storeResult(s,null,t.getMessage());
                }
                queryDone(s);

            }
        });
        thread.setDaemon(true);
        thread.setName("ChartListFetch");
        thread.start();
    }
    public synchronized void stop(){
        sequence++;
        if (thread != null){
            thread.interrupt();
        }
    }
}
