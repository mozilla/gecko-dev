/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.net.InetAddress;
import org.apache.http.conn.util.InetAddressUtils;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Enumeration;
import java.util.Formatter;
import java.util.List;
import java.util.Timer;

import com.mozilla.SUTAgentAndroid.service.ASMozStub;
import com.mozilla.SUTAgentAndroid.service.DoCommand;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.net.Uri;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.os.Handler;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

public class SUTAgentAndroid extends Activity
    {
    final Handler mHandler = new Handler();

    public static final int START_PRG = 1959;
    MenuItem mExitMenuItem;
    Timer timer = null;

    public static String sUniqueID = null;
    public static String sLocalIPAddr = null;
    public static String sACStatus = null;
    public static String sPowerStatus = null;
    public static int    nChargeLevel = 0;
    public static int    nBatteryTemp = 0;
    public static long   nCreateTimeMillis = System.currentTimeMillis();
    public static String sTestRoot = "";

    String lineSep = System.getProperty("line.separator");
    public PrintWriter dataOut = null;

    private static boolean bNetworkingStarted = false;
    private static String RegSvrIPAddr = "";
    private static String RegSvrIPPort = "";
    private static String HardwareID = "";
    private static String Pool = "";
    private static String Abi = "";
    private static String sRegString = "";
    private static boolean LogCommands = false;

    private WifiLock wl = null;

    private BroadcastReceiver battReceiver = null;

    private TextView  tv = null;

    public boolean onCreateOptionsMenu(Menu menu)
        {
        mExitMenuItem = menu.add("Exit");
        mExitMenuItem.setIcon(android.R.drawable.ic_menu_close_clear_cancel);
        return super.onCreateOptionsMenu(menu);
        }

    public boolean onMenuItemSelected(int featureId, MenuItem item)
        {
        if (item == mExitMenuItem)
            {
            finish();
            }
        return super.onMenuItemSelected(featureId, item);
        }

    public static String getRegSvrIPAddr()
        {
        return(RegSvrIPAddr);
        }

    public void pruneCommandLog(String datestamp, String testroot)
        {

        String today = "";
        String yesterday = "";

        // test root can be null (if getTestRoot fails), handle that:
        if (testroot == null) {
            testroot = "";
        }

        try {
            SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss:SSS");
            Date dateObj = sdf.parse(datestamp);
            SimpleDateFormat sdf_file = new SimpleDateFormat("yyyy-MM-dd");

            today     = sdf_file.format(dateObj);
            yesterday = sdf_file.format(new Date(dateObj.getTime() - 1000*60*60*24));
        } catch (ParseException pe) {}

        File dir = new File(testroot);

        if (!dir.isDirectory())
            return;

        File [] files = dir.listFiles();
        if (files == null)
            return;

        for (int iter = 0; iter < files.length; iter++) {
            String fName = files[iter].getName();
            if (fName.endsWith("sutcommands.txt")) {
                if (fName.endsWith(today + "-sutcommands.txt") || fName.endsWith(yesterday + "-sutcommands.txt"))
                    continue;

                if (files[iter].delete())
                    Log.i("SUTAgentAndroid", "Deleted old command logfile: " + files[iter]);
                else
                    Log.e("SUTAgentAndroid", "Unable to delete old command logfile: " + files[iter]);
            }
        }
        }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
        {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        fixScreenOrientation();

        DoCommand dc = new DoCommand(getApplication());

        Log.i("SUTAgentAndroid", dc.prgVersion);
        dc.FixDataLocalPermissions();

        // Get configuration settings from "ini" file
        File dir = getFilesDir();
        File iniFile = new File(dir, "SUTAgent.ini");
        String sIniFile = iniFile.getAbsolutePath();

        String lc = dc.GetIniData("General", "LogCommands", sIniFile);
        if (lc != "" && Integer.parseInt(lc) == 1) {
            SUTAgentAndroid.LogCommands = true;
        }
        SUTAgentAndroid.RegSvrIPAddr = dc.GetIniData("Registration Server", "IPAddr", sIniFile);
        SUTAgentAndroid.RegSvrIPPort = dc.GetIniData("Registration Server", "PORT", sIniFile);
        SUTAgentAndroid.HardwareID = dc.GetIniData("Registration Server", "HARDWARE", sIniFile);
        SUTAgentAndroid.Pool = dc.GetIniData("Registration Server", "POOL", sIniFile);
        SUTAgentAndroid.sTestRoot = dc.GetIniData("Device", "TestRoot", sIniFile);
        SUTAgentAndroid.Abi = android.os.Build.CPU_ABI;
        log(dc, "onCreate");

        dc.SetTestRoot(SUTAgentAndroid.sTestRoot);

        Log.i("SUTAgentAndroid", "Test Root: " + SUTAgentAndroid.sTestRoot);

        tv = (TextView) this.findViewById(R.id.Textview01);

        if (getLocalIpAddress() == null)
            setUpNetwork(sIniFile);

        String macAddress = "Unknown";
        if (android.os.Build.VERSION.SDK_INT > 8) {
            try {
                NetworkInterface iface = NetworkInterface.getByInetAddress(InetAddress.getAllByName(getLocalIpAddress())[0]);
                if (iface != null)
                    {
                        byte[] mac = iface.getHardwareAddress();
                        if (mac != null)
                            {
                                StringBuilder sb = new StringBuilder();
                                Formatter f = new Formatter(sb);
                                for (int i = 0; i < mac.length; i++)
                                    {
                                        f.format("%02x%s", mac[i], (i < mac.length - 1) ? ":" : "");
                                    }
                                macAddress = sUniqueID = sb.toString();
                            }
                    }
            }
            catch (UnknownHostException ex) {}
            catch (SocketException ex) {}
        }
        else
            {
                // Fall back to getting info from wifiman on older versions of Android,
                // which don't support the NetworkInterface interface
                WifiManager wifiMan = (WifiManager)getSystemService(Context.WIFI_SERVICE);
                if (wifiMan != null)
                    {
                        WifiInfo wifi = wifiMan.getConnectionInfo();
                        if (wifi != null)
                            macAddress = wifi.getMacAddress();
                        if (macAddress != null)
                            sUniqueID = macAddress;
                    }
            }

        if (sUniqueID == null)
            {
            BluetoothAdapter ba = BluetoothAdapter.getDefaultAdapter();
            if ((ba != null) && (ba.isEnabled() != true))
                {
                ba.enable();
                while(ba.getState() != BluetoothAdapter.STATE_ON)
                    {
                    try {
                        Thread.sleep(1000);
                        }
                    catch (InterruptedException e)
                        {
                        e.printStackTrace();
                        }
                    }

                sUniqueID = ba.getAddress();

                ba.disable();
                while(ba.getState() != BluetoothAdapter.STATE_OFF)
                    {
                    try {
                        Thread.sleep(1000);
                        }
                    catch (InterruptedException e)
                        {
                        e.printStackTrace();
                        }
                    }
                }
            else
                {
                if (ba != null)
                    {
                    sUniqueID = ba.getAddress();
                    sUniqueID.toLowerCase();
                    }
                }
            }

        if (sUniqueID == null)
            {
            TelephonyManager mTelephonyMgr = (TelephonyManager)getSystemService(TELEPHONY_SERVICE);
            if (mTelephonyMgr != null)
                {
                sUniqueID = mTelephonyMgr.getDeviceId();
                if (sUniqueID == null)
                    {
                    sUniqueID = "0011223344556677";
                    }
                }
            }

        String hwid = getHWID(this);

        sLocalIPAddr = getLocalIpAddress();
        Toast.makeText(getApplication().getApplicationContext(), "SUTAgent [" + sLocalIPAddr + "] ...", Toast.LENGTH_LONG).show();

        String sConfig = dc.prgVersion + lineSep;
        sConfig += "Test Root: " + sTestRoot + lineSep;
        sConfig += "Unique ID: " + sUniqueID + lineSep;
        sConfig += "HWID: " + hwid + lineSep;
        sConfig += "ABI: " + Abi + lineSep;
        sConfig += "OS Info" + lineSep;
        sConfig += "\t" + dc.GetOSInfo() + lineSep;
        sConfig += "Screen Info" + lineSep;
        int [] xy = dc.GetScreenXY();
        sConfig += "\t Width: " + xy[0] + lineSep;
        sConfig += "\t Height: " + xy[1] + lineSep;
        sConfig += "Memory Info" + lineSep;
        sConfig += "\t" + dc.GetMemoryInfo() + lineSep;
        sConfig += "Network Info" + lineSep;
        sConfig += "\tMac Address: " + macAddress + lineSep;
        sConfig += "\tIP Address: " + sLocalIPAddr + lineSep;

        displayStatus(sConfig);

        sRegString = "NAME=" + sUniqueID;
        sRegString += "&IPADDR=" + sLocalIPAddr;
        sRegString += "&CMDPORT=" + 20701;
        sRegString += "&DATAPORT=" + 20700;
        sRegString += "&OS=Android-" + dc.GetOSInfo();
        sRegString += "&SCRNWIDTH=" + xy[0];
        sRegString += "&SCRNHEIGHT=" + xy[1];
        sRegString += "&BPP=8";
        sRegString += "&MEMORY=" + dc.GetMemoryConfig();
        sRegString += "&HARDWARE=" + HardwareID;
        sRegString += "&POOL=" + Pool;
        sRegString += "&ABI=" + Abi;

        String sTemp = Uri.encode(sRegString,"=&");
        sRegString = "register " + sTemp;

        pruneCommandLog(dc.GetSystemTime(), dc.GetTestRoot());

        if (!bNetworkingStarted)
            {
            Thread thread = new Thread(null, doStartService, "StartServiceBkgnd");
            thread.start();
            bNetworkingStarted = true;

            Thread thread2 = new Thread(null, doRegisterDevice, "RegisterDeviceBkgnd");
            thread2.start();
            }

        monitorBatteryState();

        // If we are returning from an update let'em know we're back
        Thread thread3 = new Thread(null, doUpdateCallback, "UpdateCallbackBkgnd");
        thread3.start();

        final Button goButton = (Button) findViewById(R.id.Button01);
        goButton.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                finish();
                }
            });
        }

    private class UpdateStatus implements Runnable {
        public String sText = "";

        UpdateStatus(String sStatus) {
            sText = sStatus;
        }

        public void run() {
            displayStatus(sText);
        }
    }

    public synchronized void displayStatus(String sStatus) {
        String sTVText = (String) tv.getText();
        sTVText += sStatus;
        tv.setText(sTVText);
    }

    public void fixScreenOrientation()
        {
        setRequestedOrientation((getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) ?
                                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        }

    protected void onActivityResult(int requestCode, int resultCode, Intent data)
        {
        if (requestCode == START_PRG)
            {
               Toast.makeText(getApplication().getApplicationContext(), "SUTAgent startprg finished ...", Toast.LENGTH_LONG).show();
            }
        }

    @Override
    public void onDestroy()
        {
        DoCommand dc = new DoCommand(getApplication());
        super.onDestroy();
        if (isFinishing())
            {
            log(dc, "onDestroy - finishing");
            Intent listenerSvc = new Intent(this, ASMozStub.class);
            listenerSvc.setAction("com.mozilla.SUTAgentAndroid.service.LISTENER_SERVICE");
            stopService(listenerSvc);
            bNetworkingStarted = false;

            unregisterReceiver(battReceiver);

            if (wl != null)
                wl.release();

            System.exit(0);
            }
        else
            {
            log(dc, "onDestroy - not finishing");
            }
        }

    private void logMemory(String caller)
        {
        DoCommand dc = new DoCommand(getApplication());
        if (dc != null)
            {
            log(dc, caller);
            log(dc, dc.GetMemoryInfo());
            String procInfo = dc.GetProcessInfo();
            if (procInfo != null)
                {
                String lines[] = procInfo.split("\n");
                for (String line : lines) 
                    {
                    if (line.contains("mozilla"))
                        {
                        log(dc, line);
                        String words[] = line.split("\t");
                        if ((words != null) && (words.length > 1))
                            {
                            log(dc, dc.StatProcess(words[1]));
                            }
                        }
                    }
                }
            }
        else
            {
            Log.e("SUTAgentAndroid", "logMemory: unable to log to file!");
            }
        }

    @Override
    public void onLowMemory()
        {
        System.gc();
        logMemory("onLowMemory");
        }

    @Override
    public void onTrimMemory(int level)
        {
        System.gc();
        logMemory("onTrimMemory"+level);
        }

    private void monitorBatteryState()
        {
        battReceiver = new BroadcastReceiver()
            {
            public void onReceive(Context context, Intent intent)
                {
                StringBuilder sb = new StringBuilder();

                int rawlevel = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1); // charge level from 0 to scale inclusive
                int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1); // Max value for charge level
                int status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
                int health = intent.getIntExtra(BatteryManager.EXTRA_HEALTH, -1);
                boolean present = intent.getBooleanExtra(BatteryManager.EXTRA_PRESENT, false);
                int plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1); //0 if the device is not plugged in; 1 if plugged into an AC power adapter; 2 if plugged in via USB.
//                int voltage = intent.getIntExtra(BatteryManager.EXTRA_VOLTAGE, -1); // voltage in millivolts
                nBatteryTemp = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, -1); // current battery temperature in tenths of a degree Centigrade
//                String technology = intent.getStringExtra(BatteryManager.EXTRA_TECHNOLOGY);

                nChargeLevel = -1;  // percentage, or -1 for unknown
                if (rawlevel >= 0 && scale > 0)
                    {
                    nChargeLevel = (rawlevel * 100) / scale;
                    }

                if (plugged > 0)
                    sACStatus = "ONLINE";
                else
                    sACStatus = "OFFLINE";

                if (present == false)
                    sb.append("NO BATTERY");
                else
                    {
                    if (nChargeLevel < 10)
                        sb.append("Critical");
                    else if (nChargeLevel < 33)
                        sb.append("LOW");
                    else if (nChargeLevel > 80)
                        sb.append("HIGH");
                    }

                if (BatteryManager.BATTERY_HEALTH_OVERHEAT == health)
                    {
                    sb.append("Overheated ");
                    sb.append((((float)(nBatteryTemp))/10));
                    sb.append("(C)");
                    }
                else
                    {
                    switch(status)
                        {
                        case BatteryManager.BATTERY_STATUS_UNKNOWN:
                            // old emulator; maybe also when plugged in with no battery
                            if (present == true)
                                sb.append(" UNKNOWN");
                            break;
                        case BatteryManager.BATTERY_STATUS_CHARGING:
                            sb.append(" CHARGING");
                            break;
                        case BatteryManager.BATTERY_STATUS_DISCHARGING:
                            sb.append(" DISCHARGING");
                            break;
                        case BatteryManager.BATTERY_STATUS_NOT_CHARGING:
                            sb.append(" NOTCHARGING");
                            break;
                        case BatteryManager.BATTERY_STATUS_FULL:
                            sb.append(" FULL");
                            break;
                        default:
                            if (present == true)
                                sb.append("Unknown");
                            break;
                        }
                    }

                sPowerStatus = sb.toString();
                }
            };

        IntentFilter battFilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        registerReceiver(battReceiver, battFilter);
        }

    public boolean setUpNetwork(String sIniFile)
        {
        boolean    bRet = false;
        int    lcv    = 0;
        int    lcv2 = 0;
        WifiManager wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        WifiConfiguration wc = new WifiConfiguration();
        DoCommand tmpdc = new DoCommand(getApplication());

        String ssid = tmpdc.GetIniData("Network Settings", "SSID", sIniFile);
        String auth = tmpdc.GetIniData("Network Settings", "AUTH", sIniFile);
        String encr = tmpdc.GetIniData("Network Settings", "ENCR", sIniFile);
        String key = tmpdc.GetIniData("Network Settings", "KEY", sIniFile);
        String eap = tmpdc.GetIniData("Network Settings", "EAP", sIniFile);
        String adhoc = tmpdc.GetIniData("Network Settings", "ADHOC", sIniFile);

        Toast.makeText(getApplication().getApplicationContext(), "Starting and configuring network", Toast.LENGTH_LONG).show();
/*
        ContentResolver cr = getContentResolver();
        int nRet;
        try {
            nRet = Settings.System.getInt(cr, Settings.System.WIFI_USE_STATIC_IP);
            String foo2 = "" + nRet;
        } catch (SettingNotFoundException e1) {
            e1.printStackTrace();
        }
*/
/*
        wc.SSID = "\"Mozilla-Build\"";
        wc.preSharedKey  = "\"MozillaBuildQA500\"";
        wc.hiddenSSID = true;
        wc.status = WifiConfiguration.Status.ENABLED;
        wc.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.OPEN);
        wc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
        wc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
        wc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
        wc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
        wc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
        wc.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
*/
        wc.SSID = "\"" + ssid + "\"";
//        wc.SSID = "\"Mozilla-G\"";
//        wc.SSID = "\"Mozilla\"";

        if (auth.contentEquals("wpa2"))
            {
            wc.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
            wc.preSharedKey  = null;
            }

        if (encr.contentEquals("aes"))
            {
            wc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
            wc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
            }

        if (eap.contentEquals("peap"))
            {
            wc.eap.setValue("PEAP");
            wc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_EAP);
            wc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.IEEE8021X);
            }

        wc.status = WifiConfiguration.Status.ENABLED;

        if (!wifi.isWifiEnabled())
            wifi.setWifiEnabled(true);

        while(wifi.getWifiState() != WifiManager.WIFI_STATE_ENABLED)
            {
            Thread.yield();
            if (++lcv > 10000)
                return(bRet);
            }

        wl = wifi.createWifiLock(WifiManager.WIFI_MODE_FULL, "SUTAgent");
        if (wl != null)
            wl.acquire();

        WifiConfiguration    foo = null;
        int                    nNetworkID = -1;

        List<WifiConfiguration> connsLst =  wifi.getConfiguredNetworks();
        int nConns = connsLst.size();
        for (int i = 0; i < nConns; i++)
            {

            foo = connsLst.get(i);
            if (foo.SSID.equalsIgnoreCase(wc.SSID))
                {
                nNetworkID = foo.networkId;
                wc.networkId = foo.networkId;
                break;
                }
            }

        int res;

        if (nNetworkID != -1)
            {
            res = wifi.updateNetwork(wc);
            }
        else
            {
            res = wifi.addNetwork(wc);
            }

        Log.d("WifiPreference", "add Network returned " + res );

        boolean b = wifi.enableNetwork(res, true);
        Log.d("WifiPreference", "enableNetwork returned " + b );

        wifi.saveConfiguration();

        WifiInfo wi = wifi.getConnectionInfo();
        SupplicantState ss = wi.getSupplicantState();

        lcv = 0;
        lcv2 = 0;

        while (ss.compareTo(SupplicantState.COMPLETED) != 0)
            {
            try {
                Thread.sleep(1000);
                }
            catch (InterruptedException e)
                {
                e.printStackTrace();
                }

            if (wi != null)
                wi = null;
            if (ss != null)
                ss = null;
            wi = wifi.getConnectionInfo();
            ss = wi.getSupplicantState();
            if (++lcv > 60)
                {
                if (++lcv2 > 5)
                    {
                    Toast.makeText(getApplication().getApplicationContext(), "Unable to start and configure network", Toast.LENGTH_LONG).show();
                    return(bRet);
                    }
                else
                    {
                    Toast.makeText(getApplication().getApplicationContext(), "Resetting wifi interface", Toast.LENGTH_LONG).show();
                    if (wl != null)
                        wl.release();
                    wifi.setWifiEnabled(false);
                    while(wifi.getWifiState() != WifiManager.WIFI_STATE_DISABLED)
                        {
                        Thread.yield();
                        }

                    wifi.setWifiEnabled(true);
                    while(wifi.getWifiState() != WifiManager.WIFI_STATE_ENABLED)
                        {
                        Thread.yield();
                        }
                    b = wifi.enableNetwork(res, true);
                    Log.d("WifiPreference", "enableNetwork returned " + b );
                    if (wl != null)
                        wl.acquire();
                    lcv = 0;
                    }
                }
            }

        lcv = 0;
        while(getLocalIpAddress() == null)
            {
            if (++lcv > 10000)
                return(bRet);
            }

        Toast.makeText(getApplication().getApplicationContext(), "Network started and configured", Toast.LENGTH_LONG).show();
        bRet = true;

        return(bRet);
        }

    // If there is an update.info file callback the server and send the status
    private Runnable doUpdateCallback = new Runnable() {
        public void run() {
            DoCommand dc = new DoCommand(getApplication());
            String sRet = dc.UpdateCallBack("update.info");
            if (sRet.length() > 0) {
                if (sRet.contains("ok")) {
                    sRet = "Callback Server contacted successfully" + lineSep;
                } else if (sRet.contains("Nothing to do")) {
                    sRet = "";
                } else {
                    sRet = "Callback Server NOT contacted successfully" + lineSep;
                }
            }
            if (sRet.length() > 0)
                mHandler.post(new UpdateStatus(sRet));
            dc = null;
        }
    };

    // registers with the reg server defined in the SUTAgent.ini file
    private Runnable doRegisterDevice = new Runnable() {
        public void run() {
            DoCommand dc = new DoCommand(getApplication());
            String sRet = "";
            if (RegSvrIPAddr.length() > 0) {
                String sRegRet = dc.RegisterTheDevice(RegSvrIPAddr, RegSvrIPPort, sRegString);
                if (sRegRet.contains("ok")) {
                    sRet += "Registered with testserver" + lineSep;
                    sRet += "\tIPAddress: " + RegSvrIPAddr + lineSep;
                    if (RegSvrIPPort.length() > 0)
                        sRet += "\tPort: " + RegSvrIPPort + lineSep;
                } else {
                    sRet += "Not registered with testserver" + lineSep;
                }
            } else {
                sRet += "Not registered with testserver" + lineSep;
            }

        if (sRet.length() > 0)
            mHandler.post(new UpdateStatus(sRet));
        dc = null;
        }
    };

    // this starts the listener service for the command and data channels
    private Runnable doStartService = new Runnable()
        {
        public void run()
            {
            Intent listenerService = new Intent();
            listenerService.setAction("com.mozilla.SUTAgentAndroid.service.LISTENER_SERVICE");
            startService(listenerService);
            }
        };

    static String sHWID = null;
    public static String getHWID(Context cx) {
        if (sHWID != null)
            return sHWID;

        // If we're on SDK version > 8, use Build.SERIAL
        if (android.os.Build.VERSION.SDK_INT > 8) {
            sHWID = android.os.Build.SERIAL;
        }

        if (sHWID != null)
            return sHWID;

        // Otherwise, try from the telephony manager
        TelephonyManager mTelephonyMgr = (TelephonyManager) cx.getSystemService(TELEPHONY_SERVICE);
        if (mTelephonyMgr != null) {
            sHWID = mTelephonyMgr.getDeviceId();
        }

        if (sHWID != null)
            return sHWID;

        // Otherwise, try WIFI_SERVICE and use the wifi manager
        WifiManager wifiMan = (WifiManager) cx.getSystemService(Context.WIFI_SERVICE);
        if (wifiMan != null) {
            WifiInfo wifi = wifiMan.getConnectionInfo();
            if (wifi != null) {
                sHWID = "wifimac" + wifi.getMacAddress();
            }
        }

        if (sHWID != null)
            return sHWID;

        sHWID = "0011223344556677";

        return sHWID;
    }

    public static InetAddress getLocalInetAddress() throws SocketException
        {
        for (Enumeration<NetworkInterface> en = NetworkInterface.getNetworkInterfaces(); en.hasMoreElements();)
            {
            NetworkInterface intf = en.nextElement();
            for (Enumeration<InetAddress> enumIpAddr = intf.getInetAddresses(); enumIpAddr.hasMoreElements();)
                {
                InetAddress inetAddress = enumIpAddr.nextElement();
                if (!inetAddress.isLoopbackAddress() && InetAddressUtils.isIPv4Address(inetAddress.getHostAddress()))
                    {
                        return inetAddress;
                    }
                }
            }

        return null;
        }

    public String getLocalIpAddress()
        {
        try {
            InetAddress inetAddress = getLocalInetAddress();
            if (inetAddress != null)
                return inetAddress.getHostAddress().toString();
            }
        catch (SocketException ex)
            {
            Toast.makeText(getApplication().getApplicationContext(), ex.toString(), Toast.LENGTH_LONG).show();
            }
        return null;
        }

    public static void log(DoCommand dc, String message)
        {
        Log.i("SUTAgentAndroid", message);

        if (SUTAgentAndroid.LogCommands == false)
            {
            return;
            }

        if (message == null)
            {
            Log.e("SUTAgentAndroid", "bad arguments in log()!");
            return;
            }
        String fileDateStr = "00";
        String testRoot = dc.GetTestRoot();
        String datestamp = dc.GetSystemTime();
        if (testRoot == null || datestamp == null)
            {
            Log.e("SUTAgentAndroid", "Unable to get testRoot or datestamp in log!");
            return;
            }


        try 
            {
            SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss:SSS");
            Date dateStr = sdf.parse(datestamp);
            SimpleDateFormat sdf_file = new SimpleDateFormat("yyyy-MM-dd");
            fileDateStr = sdf_file.format(dateStr);
            } 
        catch (ParseException pe) {}
        String logFile = testRoot + "/" + fileDateStr + "-sutcommands.txt";
        PrintWriter pw = null;
        try 
            {
            pw = new PrintWriter(new FileWriter(logFile, true));
            pw.println(datestamp + " : " + message);
            } 
            catch (IOException ioe) 
            {
                Log.e("SUTAgentAndroid", "exception with file writer on: " + logFile);
            } 
            finally 
            {
                if (pw != null)
                {
                    pw.close();
                }
            }

        }
}
