package com.baller.demo.asr_tts_websocket;

import android.os.Message;
import android.util.Base64;
import android.util.Log;

import org.java_websocket.client.WebSocketClient;
import org.java_websocket.drafts.Draft_17;
import org.java_websocket.handshake.ServerHandshake;
import org.json.JSONException;
import org.json.JSONObject;

import java.net.URI;
import java.net.URISyntaxException;
import java.net.URLEncoder;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.HashMap;
import java.util.Locale;
import java.util.TimeZone;
import java.util.concurrent.atomic.AtomicBoolean;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

// 添加依赖：org.java-websocket:Java-WebSocket:1.3.0

public class BallerMTWebSocketTest extends Thread {

    private static String mLogTag = "BallerMTWebSocketTest";
    private static String mUrl = "ws://api.baller-tech.com/v1/service/ws/v1/mt";
    private static String mHost = "api.baller-tech.com";
    private static long mAppId = 0L;
    private static String mAppkey = "";

    private WebSocketClient mWSClient = null;
    private String mLanguage = "";
    private String mTxt = "";
    private AtomicBoolean mFinish = new AtomicBoolean(false);

    public BallerMTWebSocketTest(String strLanguage, String strTxt) {
        this.mLanguage = strLanguage;
        this.mTxt = strTxt;
    }

    private void sendNetDisconnect() {
        Message msg = Message.obtain();
        msg.what = 3002;
    }

    private static String HMACSHA256AndBase64(byte[] data, byte[] key)
    {
        try  {
            SecretKeySpec signingKey = new SecretKeySpec(key, "HmacSHA256");
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(signingKey);
            return Base64.encodeToString(mac.doFinal(data), Base64.NO_WRAP);
        } catch (Exception e) {
            e.printStackTrace();
        }

        return null;
    }

    private String makeAuthorization() {
        Calendar cd = Calendar.getInstance();
        SimpleDateFormat sdf = new SimpleDateFormat("EEE d MMM yyyy HH:mm:ss 'GMT'", Locale.US);
        sdf.setTimeZone(TimeZone.getTimeZone("GMT"));
        String strRequestDateTime = sdf.format(cd.getTime());

        String strSignatureOrg = "app_id:" + mAppId + "\n";
        strSignatureOrg += "date:" + strRequestDateTime + "\n";
        strSignatureOrg += "host:" + mHost;
        String strSignatureFinal = HMACSHA256AndBase64(strSignatureOrg.getBytes(), mAppkey.getBytes());

        JSONObject jsonAuthorization = new JSONObject();
        try {
            jsonAuthorization.put("app_id", Long.toString(mAppId));
            jsonAuthorization.put("signature", strSignatureFinal);
        } catch (JSONException e) {
            e.printStackTrace();
            return "";
        }

        String strAuthorizationFinal = Base64.encodeToString(jsonAuthorization.toString().getBytes(), Base64.NO_WRAP);
        return "authorization=" + strAuthorizationFinal + "&host=" + mHost + "&date=" + strRequestDateTime;
    }

    private static String makeFrame(String strLanguage, String strText)
    {
        JSONObject jsonParams = new JSONObject();
        JSONObject jsonBusiness = new JSONObject();
        try {
            jsonBusiness.put("language", strLanguage);
            jsonParams.put("business", jsonBusiness);
        } catch (Exception e) {
            Log.e(mLogTag, e.toString());
            return "";
        }

        JSONObject jsonData = new JSONObject();
        try {
            jsonData.put("txt", Base64.encodeToString(strText.getBytes(), Base64.NO_WRAP));
            jsonParams.put("data", jsonData);
        } catch (Exception e) {
            e.printStackTrace();
        }

        return jsonParams.toString();
    }

    @Override
    public void run() {
        final String strLanguage = this.mLanguage;
        final String strText = this.mTxt;
        String strUrl = mUrl + "?" + URLEncoder.encode(makeAuthorization());
        try
        {
            mWSClient = new WebSocketClient(new URI(strUrl), new Draft_17(), new HashMap<String, String>(), 3000) {
                @Override
                public void onOpen(ServerHandshake handshakedata) {
                    Thread sendThread = new Thread(new Runnable() {
                        @Override
                        public
                        void run() {
                            try {
                                mWSClient.send(BallerMTWebSocketTest.makeFrame(strLanguage, strText));
                            } catch (Exception e) {
                                sendNetDisconnect();

                                try {
                                    closeBlocking();
                                } catch (Exception ex ) {
                                    ex.printStackTrace();
                                }
                            }
                        }
                    });
                    sendThread.start();
                }

                @Override
                public void onMessage(String message) {
                    JSONObject jsonResult;
                    try {
                        jsonResult = new JSONObject(message);
                    } catch (Exception e) {
                        Log.i(mLogTag, e.toString());
                        return;
                    }

                    boolean isEnd = false;
                    String strData = "";
                    int error_code = 0 ;

                    try {
                        isEnd = 1 == jsonResult.getInt("is_end");
                        strData = jsonResult.getString("data");
                        error_code = jsonResult.getInt("code");
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                    if (error_code != 0)
                    {
                        Log.i(mLogTag, "mt failed: " + error_code);
                    } else {
                        Log.i(mLogTag, "mt result: " + strData);
                    }

                    if (isEnd) {
                        try {
                            mFinish.set(true);
                            Message msg = Message.obtain();
                            msg.what = 12;

                            mWSClient.closeBlocking();
                        } catch (Exception e) {
                            Log.i(mLogTag, e.toString());
                        }
                    }
                }

                @Override
                public void onClose(int code, String reason, boolean remote) {
                    mWSClient = null;
                }

                @Override
                public void onError(Exception ex) {
                    Log.i(mLogTag, "on error");
                }
            };
        } catch (URISyntaxException e)
        {
            e.printStackTrace();
            return;
        }

        boolean bConnectSucc = true;
        try {
            bConnectSucc = mWSClient.connectBlocking();
        } catch (InterruptedException e) {
            bConnectSucc = false;
            e.printStackTrace();
        }

        if (!bConnectSucc) {
            sendNetDisconnect();
        }
    }
}
