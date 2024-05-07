package it.gui.yass;

public class YassUtils {

    public static native String getServerHost();

    public static native String getServerSNI();

    public static native int getServerPort();

    public static native String getUsername();

    public static native String getPassword();

    public static native int getCipher();

    public static native String[] getCipherStrings();

    public static native String getDoHUrl();

    public static native String getDoTHost();

    public static native int getTimeout();

    public static native String saveConfig(String serverHost, String serverSNI, String serverPort,
                                     String username, String password, int cipher, String doh_url,
                                     String dot_host, String timeout);

    public static native void setEnablePostQuantumKyber(boolean enable_post_quantum_kyber);
}
