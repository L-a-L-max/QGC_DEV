package org.mavlink.qgroundcontrol;

import android.app.Activity;
import android.util.Log;

import java.util.Arrays;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

import com.skydroid.rcsdk.KeyManager;
import com.skydroid.rcsdk.RCSDKManager;
import com.skydroid.rcsdk.SDKManagerCallBack;
import com.skydroid.rcsdk.common.callback.CompletionCallbackWith;
import com.skydroid.rcsdk.common.error.SkyException;
import com.skydroid.rcsdk.key.AirLinkKey;
import com.skydroid.rcsdk.key.RemoteControllerKey;

/**
 * Manages the Skydroid RCSDK lifecycle and provides static accessors
 * so that Qt/JNI code can read the latest joystick channel values
 * without directly coupling to the RCSDK asynchronous API.
 *
 * Designed for G-series remotes (G12, G16, G20, G30) that require
 * polling (GET) for channel values, not LISTEN.
 */
public class SkydroidRCSDKManager {
    private static final String TAG = "SkydroidRCSDK";
    private static final int POLL_INTERVAL_MS = 100;
    private static final int NUM_CHANNELS = 16;

    private static volatile boolean sInitialized = false;
    private static volatile boolean sRCConnected = false;
    private static volatile String sStatusText = "not initialized";

    // Channel values: indices 0..15 correspond to CH1..CH16
    // Typical range: 1000-2000 (center ~1500)
    private static final int[] sChannelValues = new int[NUM_CHANNELS];
    private static volatile String sChannelsText = "";

    // Signal quality
    private static volatile String sSignalInfoJson = "{}";

    private static ScheduledExecutorService sExecutor;
    private static ScheduledFuture<?> sPollFuture;

    private static final SDKManagerCallBack sSdkCallback = new SDKManagerCallBack() {
        @Override
        public void onRcConnected() {
            Log.i(TAG, "RC connected");
            sRCConnected = true;
            sStatusText = "rc connected";
            startPolling();
        }

        @Override
        public void onRcConnectFail(SkyException e) {
            Log.e(TAG, "RC connect fail: " + (e != null ? e.getMessage() : "unknown"));
            sStatusText = "rc connect fail: " + (e != null ? e.getMessage() : "unknown");
        }

        @Override
        public void onRcDisconnect() {
            Log.w(TAG, "RC disconnected");
            sRCConnected = false;
            sStatusText = "rc disconnected";
            stopPolling();
        }
    };

    private static final CompletionCallbackWith<int[]> sChannelsCallback =
            new CompletionCallbackWith<int[]>() {
        @Override
        public void onSuccess(int[] channels) {
            if (channels == null) return;
            synchronized (sChannelValues) {
                int len = Math.min(channels.length, NUM_CHANNELS);
                System.arraycopy(channels, 0, sChannelValues, 0, len);
            }
            sChannelsText = Arrays.toString(channels);
        }

        @Override
        public void onFailure(SkyException e) {
            // Silently ignore transient failures; previous values are retained
        }
    };

    /**
     * Initialize the RCSDK. Call from QGCActivity.onCreate().
     */
    public static void initialize(Activity activity) {
        if (sInitialized) return;
        sInitialized = true;
        sStatusText = "initializing...";
        Log.i(TAG, "Initializing RCSDK for Skydroid G-series");
        try {
            RCSDKManager.INSTANCE.initSDK(activity, sSdkCallback);
            RCSDKManager.INSTANCE.setMainThreadCallBack(true);
            RCSDKManager.INSTANCE.connectToRC();
        } catch (Exception e) {
            Log.e(TAG, "RCSDK initSDK exception", e);
            sStatusText = "init exception: " + e.getMessage();
            sInitialized = false;
        }
    }

    /**
     * Shutdown the RCSDK. Call from QGCActivity.onDestroy().
     */
    public static void shutdown() {
        stopPolling();
        try {
            RCSDKManager.INSTANCE.disconnectRC();
        } catch (Exception e) {
            Log.e(TAG, "RCSDK disconnect exception", e);
        }
        sRCConnected = false;
        sInitialized = false;
        sStatusText = "shutdown";
    }

    private static void startPolling() {
        if (sExecutor == null) {
            sExecutor = Executors.newSingleThreadScheduledExecutor(r -> {
                Thread t = new Thread(r, "SkydroidRCPoll");
                t.setDaemon(true);
                return t;
            });
        }
        if (sPollFuture == null || sPollFuture.isCancelled()) {
            sPollFuture = sExecutor.scheduleAtFixedRate(
                    SkydroidRCSDKManager::pollChannels,
                    0, POLL_INTERVAL_MS, TimeUnit.MILLISECONDS);
            Log.i(TAG, "Channel polling started at " + POLL_INTERVAL_MS + "ms interval");
        }
    }

    private static void stopPolling() {
        if (sPollFuture != null) {
            sPollFuture.cancel(false);
            sPollFuture = null;
            Log.i(TAG, "Channel polling stopped");
        }
    }

    private static void pollChannels() {
        try {
            KeyManager.INSTANCE.get(
                    RemoteControllerKey.INSTANCE.getKeyChannels(),
                    sChannelsCallback);
        } catch (Exception e) {
            // SDK not ready or disconnected
        }
    }

    // ========================================================================
    // Static accessors for JNI (called from C++ via QJniObject)
    // ========================================================================

    /** @return true if RCSDK reports the remote controller is connected */
    public static boolean isRCConnected() {
        return sRCConnected;
    }

    /** @return human-readable SDK/connection status */
    public static String getStatusText() {
        return sStatusText;
    }

    /**
     * @return channel values as a comma-separated string, e.g. "1500,1500,1000,1500,..."
     *         Returns empty string if no data yet.
     */
    public static String getChannelsText() {
        return sChannelsText;
    }

    /**
     * @return a copy of the raw channel values array (length 16).
     *         Index 0 = CH1, index 15 = CH16.
     *         Typical range: 1000-2000, center ~1500.
     */
    public static int[] getChannelValues() {
        synchronized (sChannelValues) {
            return Arrays.copyOf(sChannelValues, NUM_CHANNELS);
        }
    }

    /**
     * @return a single channel value.
     * @param channel 0-based channel index (0=CH1, 1=CH2, ...)
     */
    public static int getChannelValue(int channel) {
        if (channel < 0 || channel >= NUM_CHANNELS) return 0;
        synchronized (sChannelValues) {
            return sChannelValues[channel];
        }
    }

    /** @return number of channels (always 16 for G-series) */
    public static int getNumChannels() {
        return NUM_CHANNELS;
    }
}
