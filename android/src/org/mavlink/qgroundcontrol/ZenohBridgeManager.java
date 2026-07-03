package org.mavlink.qgroundcontrol;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Manages the zenoh-bridge-dds subprocess lifecycle on Android.
 *
 * The bridge binary is extracted from APK assets on first use, then
 * launched as a child process that connects to the remote Zenoh
 * router/peer and bridges DDS topics to/from a local CycloneDDS domain.
 *
 * DDSLink connects to the same local DDS domain as usual, so all
 * existing functionality (readers, writers, publishers) works unchanged.
 */
public class ZenohBridgeManager {
    private static final String TAG = "ZenohBridge";
    private static final String NATIVE_LIB_NAME = "libzenoh_bridge_dds.so";
    private static final String LEGACY_ASSET_NAME = "zenoh-bridge-dds";
    private static final int STARTUP_WAIT_MS = 2000;

    private static volatile Process sBridgeProcess = null;
    private static volatile boolean sRunning = false;
    private static volatile String sStatusText = "stopped";
    private static volatile String sLastError = "";

    private static Thread sStdoutThread = null;
    private static Thread sStderrThread = null;

    /**
     * Start the zenoh-bridge-dds subprocess.
     *
     * @param context  Android context (for asset extraction)
     * @param endpoint Remote Zenoh endpoint, e.g. "tcp/192.168.1.100:7447"
     * @param domainId DDS domain to bridge (must match DDSLink's domain)
     * @return true if bridge started successfully
     */
    public static boolean start(Context context, String endpoint, int domainId) {
        if (sRunning && sBridgeProcess != null) {
            Log.w(TAG, "Bridge already running");
            return true;
        }

        sLastError = "";

        final File binary = findBinary(context);
        if (binary == null) {
            sStatusText = "binary not found";
            return false;
        }

        final File configFile = writeConfig(context, endpoint, domainId);
        if (configFile == null) {
            sStatusText = "config write failed";
            return false;
        }

        try {
            ProcessBuilder pb = new ProcessBuilder(
                binary.getAbsolutePath(),
                "-c", configFile.getAbsolutePath()
            );

            pb.environment().put("RUST_LOG", "zenoh=info,zenoh::net::routing::dispatcher::pubsub=off,zenoh_plugin_dds=info");
            pb.redirectErrorStream(false);

            Log.i(TAG, "Starting bridge: " + binary.getAbsolutePath()
                    + " -c " + configFile.getAbsolutePath());

            sBridgeProcess = pb.start();
            sRunning = true;
            sStatusText = "starting...";

            sStdoutThread = new Thread(() -> readStream("stdout", sBridgeProcess.getInputStream()), "zenoh-stdout");
            sStderrThread = new Thread(() -> readStream("stderr", sBridgeProcess.getErrorStream()), "zenoh-stderr");
            sStdoutThread.setDaemon(true);
            sStderrThread.setDaemon(true);
            sStdoutThread.start();
            sStderrThread.start();

            // Wait briefly for bridge to initialize
            Thread.sleep(STARTUP_WAIT_MS);

            if (!sBridgeProcess.isAlive()) {
                int exitCode = sBridgeProcess.exitValue();
                sStatusText = "exited with code " + exitCode;
                sLastError = "Bridge process exited immediately (code " + exitCode + ")";
                Log.e(TAG, sLastError);
                sRunning = false;
                return false;
            }

            sStatusText = "running (endpoint: " + endpoint + ")";
            Log.i(TAG, "Bridge started successfully, PID active");
            return true;

        } catch (Exception e) {
            Log.e(TAG, "Failed to start bridge", e);
            sStatusText = "start failed";
            sLastError = e.getMessage();
            sRunning = false;
            return false;
        }
    }

    /**
     * Stop the zenoh-bridge-dds subprocess.
     */
    public static void stop() {
        if (sBridgeProcess != null) {
            try {
                sBridgeProcess.destroy();
                sBridgeProcess.waitFor();
            } catch (Exception e) {
                Log.e(TAG, "Error stopping bridge", e);
                sBridgeProcess.destroyForcibly();
            }
            sBridgeProcess = null;
        }
        sRunning = false;
        sStatusText = "stopped";
        Log.i(TAG, "Bridge stopped");
    }

    /**
     * Check if the bridge process is alive.
     */
    public static boolean isRunning() {
        if (sRunning && sBridgeProcess != null && !sBridgeProcess.isAlive()) {
            sRunning = false;
            sStatusText = "crashed (exit: " + sBridgeProcess.exitValue() + ")";
        }
        return sRunning;
    }

    public static String getStatusText() {
        return sStatusText;
    }

    public static String getLastError() {
        return sLastError;
    }

    /**
     * Locate the bridge binary. Prefers the native library directory
     * (jniLibs), which Android grants execute permission by default.
     * Falls back to extracting from APK assets for legacy builds.
     */
    private static File findBinary(Context context) {
        // Primary: look in nativeLibraryDir (android/libs/arm64-v8a/libzenoh_bridge_dds.so)
        // SELinux always allows execution from this directory.
        String nativeLibDir = context.getApplicationInfo().nativeLibraryDir;
        File nativeLib = new File(nativeLibDir, NATIVE_LIB_NAME);
        if (nativeLib.exists() && nativeLib.canExecute()) {
            Log.i(TAG, "Using native library: " + nativeLib.getAbsolutePath());
            return nativeLib;
        }
        if (nativeLib.exists()) {
            Log.w(TAG, "Native lib exists but not executable: " + nativeLib.getAbsolutePath());
        }

        // Fallback: extract from assets (legacy approach, may fail on Android 10+
        // due to SELinux W^X policy on app data directories)
        Log.w(TAG, "Native lib not found in " + nativeLibDir
                + ", falling back to asset extraction");
        return extractFromAssets(context);
    }

    /**
     * Legacy extraction from APK assets to app-internal storage.
     * May fail on Android 10+ devices with strict SELinux W^X policy.
     */
    private static File extractFromAssets(Context context) {
        final File binDir = new File(context.getFilesDir(), "zenoh");
        if (!binDir.exists() && !binDir.mkdirs()) {
            Log.e(TAG, "Cannot create directory: " + binDir);
            sLastError = "Cannot create directory: " + binDir;
            return null;
        }

        final File binary = new File(binDir, LEGACY_ASSET_NAME);

        if (binary.exists() && binary.canExecute()) {
            Log.i(TAG, "Binary already extracted: " + binary.getAbsolutePath());
            return binary;
        }

        try (InputStream is = context.getAssets().open(LEGACY_ASSET_NAME);
             OutputStream os = new FileOutputStream(binary)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = is.read(buf)) > 0) {
                os.write(buf, 0, n);
            }
            os.flush();
        } catch (IOException e) {
            Log.e(TAG, "Failed to extract binary from assets", e);
            sLastError = "Binary extraction failed: " + e.getMessage();
            return null;
        }

        if (!binary.setExecutable(true, false)) {
            Log.e(TAG, "Cannot set execute permission on: " + binary
                    + ". Device may enforce SELinux W^X policy."
                    + " Place the binary as " + NATIVE_LIB_NAME
                    + " in android/libs/arm64-v8a/ instead.");
            sLastError = "Permission denied: place binary as " + NATIVE_LIB_NAME
                    + " in android/libs/arm64-v8a/";
            return null;
        }

        Log.i(TAG, "Binary extracted to: " + binary.getAbsolutePath());
        return binary;
    }

    /**
     * Write the zenoh-bridge-dds JSON5 configuration file.
     */
    private static File writeConfig(Context context, String endpoint, int domainId) {
        final File configDir = new File(context.getFilesDir(), "zenoh");
        if (!configDir.exists() && !configDir.mkdirs()) {
            Log.e(TAG, "Cannot create config directory");
            sLastError = "Cannot create config directory";
            return null;
        }

        final File configFile = new File(configDir, "bridge_config.json5");

        // Build JSON5 config for zenoh-bridge-dds v1.9+
        // Valid DDS plugin fields: scope, domain, allow, deny, max_frequencies,
        // generalise_subs, generalise_pubs, forward_discovery,
        // reliable_routes_blocking, localhost_only, queries_timeout,
        // work_thread_num, max_block_thread_num

        // Only bridge topics that QGC actually uses (from dds_mappings).
        // Out: 15 topics QGC subscribes to; In: 4 topics QGC publishes.
        String allowPattern = "rt/fmu/(out/(vehicle_attitude|vehicle_global_position"
            + "|vehicle_local_position_v1|vehicle_gps_position|battery_status_v1"
            + "|wind|airspeed_validated_v1|sensor_combined|manual_control_setpoint"
            + "|estimator_status_flags|vehicle_status_v1|home_position_v1"
            + "|vehicle_land_detected|failsafe_flags|vehicle_command_ack)"
            + "|in/(vehicle_command|telemetry_status|manual_control_input|goto_setpoint))";

        String config = "{\n"
            + "  mode: \"peer\",\n"
            + "  connect: {\n"
            + "    endpoints: [\"" + endpoint + "\"],\n"
            + "    exit_on_failure: false,\n"
            + "    retry: {\n"
            + "      period_init_ms: 1000,\n"
            + "      period_max_ms: 5000,\n"
            + "      period_increase_factor: 2.0\n"
            + "    }\n"
            + "  },\n"
            + "  timestamping: { enabled: false },\n"
            + "  plugins: {\n"
            + "    dds: {\n"
            + "      domain: " + domainId + ",\n"
            + "      allow: \"" + allowPattern + "\",\n"
            + "      localhost_only: true,\n"
            + "      forward_discovery: true\n"
            + "    }\n"
            + "  }\n"
            + "}\n";

        try (FileOutputStream fos = new FileOutputStream(configFile)) {
            fos.write(config.getBytes("UTF-8"));
            fos.flush();
        } catch (IOException e) {
            Log.e(TAG, "Failed to write config file", e);
            sLastError = "Config write failed: " + e.getMessage();
            return null;
        }

        Log.i(TAG, "Config written to: " + configFile.getAbsolutePath());
        return configFile;
    }

    /**
     * Read and log output from a process stream.
     */
    private static void readStream(String label, InputStream stream) {
        try {
            byte[] buf = new byte[4096];
            int n;
            StringBuilder line = new StringBuilder();
            while ((n = stream.read(buf)) > 0) {
                String chunk = new String(buf, 0, n);
                line.append(chunk);

                // Log complete lines
                int idx;
                while ((idx = line.indexOf("\n")) >= 0) {
                    String msg = line.substring(0, idx).trim();
                    if (!msg.isEmpty()) {
                        Log.i(TAG, "[" + label + "] " + msg);
                    }
                    line.delete(0, idx + 1);
                }
            }
            // Log remaining
            if (line.length() > 0) {
                Log.i(TAG, "[" + label + "] " + line.toString().trim());
            }
        } catch (IOException e) {
            Log.w(TAG, label + " stream read ended: " + e.getMessage());
        }
    }
}
