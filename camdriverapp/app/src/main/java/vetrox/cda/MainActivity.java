package vetrox.cda;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.os.Bundle;
import android.util.Log;
import android.util.Size;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.ImageProxy;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.MulticastSocket;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.Executors;


public class MainActivity extends AppCompatActivity {

    private static final String[] permissions = {
            Manifest.permission.CAMERA,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.INTERNET,
            Manifest.permission.ACCESS_NETWORK_STATE,
            Manifest.permission.FOREGROUND_SERVICE
    };

    private static final int RES = 80; // 80=720p, 120=1080p
    private static final int WIDTH = 16 * RES, HEIGHT = 9 * RES;

    @SuppressLint("RestrictedApi")
    public static final ImageAnalysis imageAnalysis = new ImageAnalysis.Builder()
            .setTargetResolution(new Size(WIDTH, HEIGHT))
            .setCameraSelector(CameraSelector.DEFAULT_BACK_CAMERA)
            .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_YUV_420_888)
            .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST) // non blocking mode
            .build();

    private static DatagramSocket clientSocket;
    private static InetAddress serverIPAddr = null;
    static {
        try {
            clientSocket = new DatagramSocket();
        } catch (SocketException e) {
            e.printStackTrace();
        }
    }
    private static final int PORT = 50684;
    private static int last_transmission_id = 0;

    /**
     * Splits the image over several UDP-Packets if needed.
     */
    public static void sendImageFramed(final byte[] data) {
        final int PAYLOAD_SIZE = 65000; // circa
        if (data.length / PAYLOAD_SIZE + 1 >= 20) {  // too much frames
            return;
        }

        final int SIZEOF_PACK_COUNT = 1; // 8 bit
        final int SIZEOF_PACK_INDEX = 1; // 8 bit
        final int HEADER_SIZE = SIZEOF_PACK_COUNT + SIZEOF_PACK_INDEX /*+ SIZEOF_OFFSET*/;


        int transmission_id = 0;
        while(transmission_id == last_transmission_id) {
            transmission_id = (int) ((Math.random() * 0b0111_1111) + 1); // [1,127]
        }
        last_transmission_id = transmission_id;
        // Temporary to copy extra data into
        final byte[] frameBuf = new byte[HEADER_SIZE + PAYLOAD_SIZE];
        frameBuf[0] = (byte) transmission_id;

        for (int frame_index = 0, offset = 0; offset < data.length; offset += PAYLOAD_SIZE, frame_index++) {
            final int frame_size = Math.min(data.length - offset, PAYLOAD_SIZE);
            final int payloadSize = HEADER_SIZE + frame_size;

            frameBuf[1] = (byte) (frame_index);
            if (offset + PAYLOAD_SIZE >= data.length) { // last frame
                frameBuf[1] |= 0b1000_0000;
            }

            System.arraycopy(data, offset, frameBuf, HEADER_SIZE, frame_size);
            try {
                clientSocket.send(new DatagramPacket(frameBuf, 0, payloadSize, serverIPAddr, PORT));
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * Analysis of the current CameraX image
     * @param image the input image
     */
    public static void analyze(@NonNull ImageProxy image) {
        long s = System.currentTimeMillis();

        final int width = image.getWidth();
        final int height = image.getHeight();
        final ByteBuffer y = image.getPlanes()[0].getBuffer();
        final ByteBuffer u = image.getPlanes()[1].getBuffer();
        final ByteBuffer v = image.getPlanes()[2].getBuffer();

        final int ySize = y.remaining();
        final int uSize = u.remaining();
        final int vSize = v.remaining();

        final byte[] nv21 = new byte[ySize + uSize]; // 16 bit per pixel
        y.get(nv21, 0, ySize);
        for (int o = ySize; o < nv21.length - 1; o += 2) {
            nv21[o + 1] = v.get();
            nv21[o] = v.get();
        }

        image.close(); // asap

        final YuvImage yuvImage = new YuvImage(nv21, ImageFormat.NV21, width, height, null);
        final ByteArrayOutputStream baos = new ByteArrayOutputStream();
        yuvImage.compressToJpeg(new Rect(0, 0, width, height), 75, baos);

        sendImageFramed(baos.toByteArray());
    }

    public static void startRecording(ProcessCameraProvider cameraProvider, CaptureService instance) {
        /// TEST AREA (receiving multicast)
        final String    multicast_ip    = "239.123.234.45";     // see spec
        final int       multicast_port  = 50685;                // see spec
        final int       receive_buf_len = 1024;                 // see spec
        final String    v1ExpPayload    = "BEGIN----v1.0----multicast----cam----server----END";

        Log.i("MAIN", "///////////////////////////////////////////////////////");
        MulticastSocket dS = null;
        byte[] receive_buffer = new byte[receive_buf_len];
        DatagramPacket packet = new DatagramPacket(receive_buffer, receive_buf_len);

        while (serverIPAddr == null) {
            try {
                if (dS == null) {
                    dS = new MulticastSocket(multicast_port);
                    dS.joinGroup(
                            new InetSocketAddress(
                                    InetAddress.getByName(multicast_ip),
                                    multicast_port
                            ),
                            NetworkInterface.getByName("bge0")
                    );
                }

                Log.i("MAIN", "waiting for the server to send multicast");
                dS.receive(packet); // blocking
                Log.i("MAIN", "received multicast message");

                // read NULL-terminated payload
                StringBuilder cache = new StringBuilder();
                for (int i = 0; i < receive_buf_len && receive_buffer[i] != 0; i++) {
                    cache.append((char) receive_buffer[i]);
                }

                String payload = cache.toString();
                if (payload.equals(v1ExpPayload)) {
                    serverIPAddr = packet.getAddress(); // get server address
                    Log.i("MAIN", "multicast came from " + serverIPAddr.getHostName());
                }
                dS.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        if(dS != null) { // should be != null anyways but ok
            dS.close();
        }
        Log.i("MAIN", "///////////////////////////////////////////////////////");
        /// END OF TEST AREA

        imageAnalysis.setAnalyzer(Executors.newSingleThreadExecutor(), MainActivity::analyze);
        ContextCompat.getMainExecutor(context).execute(() -> {
            cameraProvider.bindToLifecycle(instance, CameraSelector.DEFAULT_BACK_CAMERA, imageAnalysis);
        });
    }

    public static void stopRecording() {
        imageAnalysis.clearAnalyzer(); // stop analysing
        if(context != null && intent != null) {
            context.stopService(intent);
        }
    }

    @SuppressLint("RestrictedApi")
    public void stopRecording(View v) {
        stopRecording();
        finish();  // Minimize / Close app
    }

    private static Intent intent;
    private static Context context;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        requestPermissions();

        //////// TEST AREA
        context = getApplicationContext();
        intent = new Intent(this, CaptureService.class); // Build the intent for the service
        context.startForegroundService(intent);
        //////// END OF TEST AREA

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopRecording();
    }

    private void requestPermissions() {
        while (true){
            if (Arrays.stream(permissions).noneMatch(perm -> {
                final boolean granted = ActivityCompat.checkSelfPermission(this, perm) == PackageManager.PERMISSION_GRANTED;
                if (!granted) {
                    ActivityCompat.requestPermissions(this, new String[]{perm}, 10);
                }
                return !granted;
            })) break;
        }
    }
}