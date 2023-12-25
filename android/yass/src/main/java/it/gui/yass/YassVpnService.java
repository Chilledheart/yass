package it.gui.yass;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.ProxyInfo;
import android.net.VpnService;
import android.os.Build;
import android.os.ParcelFileDescriptor;

public class YassVpnService extends VpnService {
    public int DEFAULT_MTU = 1500;
    private String PRIVATE_VLAN4_CLIENT = "172.19.0.1";
    private String PRIVATE_VLAN4_GATEWAY = "172.19.0.2";
    private String PRIVATE_VLAN6_CLIENT = "fdfe:dcba:9876::1";
    private String PRIVATE_VLAN6_GATEWAY = "fdfe:dcba:9876::2";

    public ParcelFileDescriptor connect(Context context) {
        Builder builder = new Builder();

        builder.setConfigureIntent(PendingIntent.getActivity(context, 0,
                new Intent(context, MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT),
                PendingIntent.FLAG_IMMUTABLE));
        builder.setSession("session");
        builder.setMtu(DEFAULT_MTU);
        builder.addAddress(PRIVATE_VLAN4_CLIENT, 30);
        builder.addAddress(PRIVATE_VLAN6_CLIENT, 126);
        builder.addRoute("0.0.0.0", 0);
        builder.addRoute("::", 0);
        // builder.setUnderlyingNetworks(underlyingNetworks);
        // builder.setMetered(metered);
        builder.addDnsServer(PRIVATE_VLAN4_GATEWAY);
        builder.addDnsServer(PRIVATE_VLAN6_GATEWAY);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            builder.setHttpProxy(ProxyInfo.buildDirectProxy("127.0.0.1", 3000));
        }
        try {
            builder.addDisallowedApplication("it.gui.yass");
        } catch (PackageManager.NameNotFoundException e) {
            // nop
        }

        return builder.establish();
    }
}
