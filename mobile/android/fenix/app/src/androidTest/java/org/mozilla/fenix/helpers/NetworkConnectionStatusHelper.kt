/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("DEPRECATION")

package org.mozilla.fenix.helpers

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.wifi.WifiManager
import android.os.Build
import android.telephony.TelephonyManager
import android.util.Log
import androidx.test.platform.app.InstrumentationRegistry
import org.mozilla.fenix.helpers.Constants.TAG

object NetworkConnectionStatusHelper {
    // Get the context of the app to access system services
    val context = InstrumentationRegistry.getInstrumentation().targetContext

    // Get the ConnectivityManager system service to manage network connections
    val connectivityManager =
        context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

    // Get a list of all networks currently available
    val allNetworks = connectivityManager.allNetworks

    // Get the active network the device is currently connected to
    val activeNetwork = connectivityManager.activeNetwork

    fun getNetworkDetails() {
        // Get the network capabilities of the active network
        val activeNetworkCapabilities = connectivityManager.getNetworkCapabilities(activeNetwork)
        // Check if the active network has capabilities and determine its type
        if (activeNetworkCapabilities != null) {
            when {
                activeNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> {
                    Log.i(TAG, "getNetworkDetails: The device's active network is: WiFi")
                }
                activeNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> {
                    Log.i(TAG, "getNetworkDetails: The device's active network is: Cellular")
                }
                else -> {
                    Log.i(TAG, "getNetworkDetails: The device's active network is: Unknown")
                }
            }
        } else {
            Log.i(TAG, "getNetworkDetails: No active network")
        }

        // Iterate through all available networks to check their capabilities
        for (network in allNetworks) {
            // Get the network capabilities for a specific network
            val networkCapabilities = connectivityManager.getNetworkCapabilities(network)
            // Check if network capabilities are available
            if (networkCapabilities != null) {
                when {
                    networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> {
                        Log.i(TAG, "getNetworkDetails: Connected to WiFi")
                        val wifiStrength = getWifiSignalStrength(context)
                        Log.i(TAG, "getNetworkDetails: WiFi Signal Strength: $wifiStrength")
                    }
                    networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> {
                        Log.i(TAG, "getNetworkDetails: Connected to Cellular Network")
                        val signalStrength = getCellularSignalStrength(context)
                        Log.i(TAG, "getNetworkDetails: Cellular Signal Strength: $signalStrength")
                    }
                    else -> {
                        Log.i(TAG, "getNetworkDetails: Connected to Unknown Network")
                    }
                }
            } else {
                Log.i(TAG, "getNetworkDetails: No Network Connection")
            }
        }
    }

    fun checkActiveNetworkState(enabled: Boolean): Boolean {
        // Get the network capabilities of the active network
        val activeNetworkCapabilities = connectivityManager.getNetworkCapabilities(activeNetwork)

        return if (activeNetworkCapabilities != null) {
            // Determine the type of active network connection
            val connectionType = when {
                activeNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> "WiFi"
                activeNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> "Cellular"
                else -> "Unknown"
            }

            // Check if the active network is connected to WiFi or Cellular
            val isConnected = activeNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) || activeNetworkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)
            Log.i(TAG, "checkActiveNetworkState: Active network is ${if (isConnected) "connected" else "not connected"} ($connectionType)")
            // Return true if the enabled parameter matches the connection state
            enabled == isConnected
        } else {
            // Log and return false if there is no active network connection
            Log.i(TAG, "checkActiveNetworkState: No active network connection")
            false
        }
    }

    // Function to get WiFi signal strength based on the RSSI (Received Signal Strength Indicator)
    fun getWifiSignalStrength(context: Context): String {
        // Get the WifiManager system service to access WiFi information
        val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        // Get the current RSSI (Received Signal Strength Indicator) value of the connected WiFi network
        val rssi = wifiManager.connectionInfo.rssi
        // Determine the WiFi signal strength based on the RSSI value
        return when {
            // If the RSSI is greater than -50 dBm, the signal is considered "Excellent"
            rssi > -50 -> "Excellent"
            // If the RSSI is between -50 and -60 dBm, the signal is considered "Good"
            rssi > -60 -> "Good"
            // If the RSSI is between -60 and -70 dBm, the signal is considered "Fair"
            rssi > -70 -> "Fair"
            // If the RSSI is less than -70 dBm, the signal is considered "Weak"
            else -> "Weak"
        }
    }

    // Function to get the cellular network signal strength based on the current connection
    fun getCellularSignalStrength(context: Context): String {
        // getSignalStrength() added only in API Level 28
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Get the TelephonyManager system service to access cellular network information
            val telephonyManager =
                context.getSystemService(Context.TELEPHONY_SERVICE) as TelephonyManager
            // Get the current signal strength object which contains signal strength information
            val signalStrength = telephonyManager.signalStrength

            // Return the signal quality based on the signal level
            return if (signalStrength != null) {
                val level = signalStrength.level
                when {
                    // Excellent signal if the level is 4 (strongest signal)
                    level >= 4 -> "Excellent"
                    // Good signal for level 3
                    level == 3 -> "Good"
                    // Fair signal for level 2
                    level == 2 -> "Fair"
                    // Weak signal for level 1 or below
                    else -> "Weak"
                }
            } else {
                "N/A"
            }
        } else {
            return ("API level is lower than 28, can't get the signal strength")
        }
    }
}
