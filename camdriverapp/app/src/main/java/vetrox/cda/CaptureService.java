package vetrox.cda;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.lifecycle.LifecycleService;

import java.util.concurrent.Executors;

public class CaptureService extends LifecycleService {

    @Override
    public void onStart(@Nullable Intent intent, int startId) {
        super.onStart(intent, startId);
        var cameraProviderFuture = ProcessCameraProvider.getInstance(this);
        cameraProviderFuture.addListener(() -> {
            try {
                MainActivity.startRecording(cameraProviderFuture.get(), this);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }, Executors.newSingleThreadExecutor());
    }

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        super.onStartCommand(intent, flags, startId);

        final String NOTIFICATION_CHANNEL_ID = "abc.def.ghi.jkl";
        final String NOT_CHANNEL_NAME = "aaaaaaaaaaa";
        final int ONGOING_NOTIFICATION_ID = 123114;

        PendingIntent pendingIntent = PendingIntent.getActivity(
                this,
                0,
                new Intent(this, CaptureService.class),
                0
        );

        NotificationChannel channel = new NotificationChannel(NOTIFICATION_CHANNEL_ID, NOT_CHANNEL_NAME, NotificationManager.IMPORTANCE_NONE);
        NotificationManager service = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        service.createNotificationChannel(channel);

        Notification notification =
                new Notification.Builder(this, NOTIFICATION_CHANNEL_ID)
                        .setContentTitle("Recording")
                        .setContentText("The back camera is recording and images are being sent")
                        .setSmallIcon(R.mipmap.ic_launcher)
                        .setContentIntent(pendingIntent)
                        .build();

        startForeground(ONGOING_NOTIFICATION_ID, notification);
        return START_NOT_STICKY; // let it die if it dies do not restart
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stopForeground(true);
        Log.i("CaptureService", "onDestroy");
    }

}
