package io.github.wszqkzqk.pvzportable;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import java.util.LinkedHashMap;
import java.util.Map;

public final class RoomClipboardBridge {
    private static final int LIMIT = 4096;
    private final Map<Integer, Result> results = new LinkedHashMap<>();
    private static final class Result {
        int state = 1;
        String text = "";
    }

    public void request(Activity activity, int id, int action, String value) {
        final Result result = new Result();
        synchronized (results) {
            while (results.size() >= 8) results.remove(results.keySet().iterator().next());
            results.put(id, result);
        }
        activity.runOnUiThread(() -> {
            synchronized (results) { if (results.get(id) != result) return; }
            String text = "";
            boolean success = false;
            try {
                if (activity.isFinishing() || activity.isDestroyed()) throw new IllegalStateException();
                if (action == 2) {
                    if (!valid(value)) throw new IllegalArgumentException();
                    Intent intent = new Intent(Intent.ACTION_SEND);
                    intent.setType("text/plain");
                    intent.putExtra(Intent.EXTRA_TEXT, value);
                    activity.startActivity(Intent.createChooser(intent, "PVZ"));
                    success = true;
                } else {
                    ClipboardManager clipboard = (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
                    if (clipboard == null) throw new IllegalStateException();
                    if (action == 1) {
                        if (!valid(value)) throw new IllegalArgumentException();
                        clipboard.setPrimaryClip(ClipData.newPlainText("PVZ", value));
                        success = true;
                    } else if (action == 0) {
                        ClipData clip = clipboard.getPrimaryClip();
                        if (clip != null && clip.getItemCount() > 0) {
                            CharSequence content = clip.getItemAt(0).getText();
                            if (content != null && content.length() <= LIMIT) text = content.toString();
                            success = valid(text);
                        }
                    }
                }
            } catch (RuntimeException unavailable) { success = false; }
            synchronized (results) {
                if (results.get(id) != result) return;
                result.text = success && action == 0 ? text : "";
                result.state = success ? 2 : 3;
            }
        });
    }

    private static boolean valid(String value) {
        return value != null && !value.isEmpty() && value.length() <= LIMIT && value.indexOf('\0') < 0;
    }
    public int status(int id) {
        synchronized (results) { Result value = results.get(id); return value == null ? 3 : value.state; }
    }
    public String text(int id) {
        synchronized (results) { Result value = results.get(id); return value == null || value.state != 2 ? "" : value.text; }
    }
    public void finish(int id) { synchronized (results) { results.remove(id); } }
    public void clear() { synchronized (results) { results.clear(); } }
}
