package org.juniperlang.cwatch;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import android.Manifest;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Toast;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.UUID;

public class MainActivity extends AppCompatActivity implements MyRecyclerViewAdapter.ItemClickListener {
    private MyRecyclerViewAdapter adapter;

    private final int REQUEST_ENABLE_BT = 1;

    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bluetoothLeScanner;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Initializes Bluetooth adapter.
        final BluetoothManager bluetoothManager =
                (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        bluetoothAdapter = bluetoothManager.getAdapter();

        bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();

        // set up the RecyclerView
        RecyclerView recyclerView = findViewById(R.id.rvBLEList);
        LinearLayoutManager layoutManager = new LinearLayoutManager(this);
        recyclerView.setLayoutManager(layoutManager);
        DividerItemDecoration dividerItemDecoration = new DividerItemDecoration(recyclerView.getContext(),
                layoutManager.getOrientation());
        recyclerView.addItemDecoration(dividerItemDecoration);
        adapter = new MyRecyclerViewAdapter(this);
        adapter.setClickListener(this);
        recyclerView.setAdapter(adapter);

        checkLocationPermission();

        // Ensures Bluetooth is available on the device and it is enabled. If not,
        // displays a dialog requesting user permission to enable Bluetooth.
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
        }
    }

    public static final int MY_PERMISSIONS_REQUEST_LOCATION = 99;

    public boolean checkLocationPermission() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {

            // Should we show an explanation?
            if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                    Manifest.permission.ACCESS_FINE_LOCATION)) {

                // Show an explanation to the user *asynchronously* -- don't block
                // this thread waiting for the user's response! After the user
                // sees the explanation, try again to request the permission.
                new AlertDialog.Builder(this)
                        .setTitle(R.string.title_location_permission)
                        .setMessage(R.string.text_location_permission)
                        .setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialogInterface, int i) {
                                //Prompt the user once explanation has been shown
                                ActivityCompat.requestPermissions(MainActivity.this,
                                        new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
                                        MY_PERMISSIONS_REQUEST_LOCATION);
                            }
                        })
                        .create()
                        .show();


            } else {
                // No explanation needed, we can request the permission.
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
                        MY_PERMISSIONS_REQUEST_LOCATION);
            }
            return false;
        } else {
            return true;
        }
    }

    private final UUID rawTimeServiceUuid = UUID.fromString("00001805-0000-1000-8000-00805f9b34fb");
    private final UUID dayDateTimeChrUuid = UUID.fromString("00002A0A-0000-1000-8000-00805f9b34fb");
    private final UUID dayOfWeekChrUuid = UUID.fromString("00002A09-0000-1000-8000-00805f9b34fb");

    private BluetoothGatt remoteGatt;

    private void writeCharacteristic(BluetoothGattCharacteristic characteristic, byte[] payload) {
        characteristic.setValue(payload);
        if ((characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_WRITE) == BluetoothGattCharacteristic.PROPERTY_WRITE) {
            characteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
        } else if ((characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) == BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) {
            characteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
        }
        remoteGatt.writeCharacteristic(characteristic);
    }

    private BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (characteristic.getUuid().equals(dayDateTimeChrUuid)) {
                    // We have succeeded in writing the day/date/time characteristic, now
                    // write the day of week
                    ByteBuffer dayOfWeek = ByteBuffer.allocate(1);
                    Calendar calendar = Calendar.getInstance();
                    dayOfWeek.put((byte) (calendar.get(Calendar.DAY_OF_WEEK) - 1));
                    BluetoothGattCharacteristic dayOfWeekChr = gatt.getService(rawTimeServiceUuid).getCharacteristic(dayOfWeekChrUuid);
                    writeCharacteristic(dayOfWeekChr, dayOfWeek.array());
                }
                Log.i("BluetoothGattCallback", "Wrote to characteristic $uuid | value: ${value.toHexString()}");
            } else if (status == BluetoothGatt.GATT_INVALID_ATTRIBUTE_LENGTH) {
                Log.e("BluetoothGattCallback", "Write exceeded connection ATT MTU!");
            } else if (status == BluetoothGatt.GATT_WRITE_NOT_PERMITTED) {
                Log.e("BluetoothGattCallback", "Write not permitted for $uuid!");
            } else {
                Log.e("BluetoothGattCallback", "Characteristic write failed for $uuid, error: $status");
            }
        }

        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            super.onConnectionStateChange(gatt, status, newState);

            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    remoteGatt = gatt;
                    gatt.discoverServices();
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    gatt.close();
                }
            } else {
                gatt.close();
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            super.onServicesDiscovered(gatt, status);

            // First write the day date time
            // Once this has succeeded, we then write the day of the week
            // in the onCharacteristicWrite callback.
            // Android BLE is not good at queueing multiple writes, so
            // we have to do the next write only after the first write has
            // succeeded
            BluetoothGattCharacteristic dayDateTimeChr = gatt.getService(rawTimeServiceUuid).getCharacteristic(dayDateTimeChrUuid);
            ByteBuffer dayDateTime = ByteBuffer.allocate(9);
            Calendar calendar = Calendar.getInstance();
            dayDateTime.put((byte) (calendar.get(Calendar.MONTH)));
            dayDateTime.put((byte) (calendar.get(Calendar.DAY_OF_MONTH)));
            dayDateTime.putInt((int) (calendar.get(Calendar.YEAR)));
            dayDateTime.put((byte) (calendar.get(Calendar.HOUR_OF_DAY)));
            dayDateTime.put((byte) (calendar.get(Calendar.MINUTE)));
            dayDateTime.put((byte) (calendar.get(Calendar.SECOND)));
            writeCharacteristic(dayDateTimeChr, dayDateTime.array());
        }
    };

    @Override
    public void onItemClick(View view, int position) {
        BluetoothDevice device = mLeDevices.get(position);
        if (device.getName().equals("CWatch")) {
            device.connectGatt(getApplicationContext(), false, gattCallback);
        }
    }

    /** Called when the user touches the button */
    public void scanPressed(View view) {
        // Do something in response to button click
        scanLeDevice();
    }

    private boolean mScanning;
    private Handler handler = new Handler(Looper.getMainLooper());

    // Stops scanning after 10 seconds.
    private static final long SCAN_PERIOD = 10000;

    private void scanLeDevice() {
        if (!mScanning) {
            // Stops scanning after a pre-defined scan period.
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    mScanning = false;
                    bluetoothLeScanner.stopScan(leScanCallback);
                }
            }, SCAN_PERIOD);

            mScanning = true;
            bluetoothLeScanner.startScan(leScanCallback);
        }
    }

    private void stopScanLeDevice() {
        if (mScanning) {
            mScanning = false;
            bluetoothLeScanner.stopScan(leScanCallback);
            handler.removeCallbacksAndMessages(null);
        }
    }

    private ArrayList<BluetoothDevice> mLeDevices = new ArrayList<BluetoothDevice>();

    // Device scan callback.
    private ScanCallback leScanCallback =
        new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                super.onScanResult(callbackType, result);
                BluetoothDevice device = result.getDevice();
                if (!mLeDevices.contains(device)) {
                    mLeDevices.add(device);
                    adapter.insertSingleItem(device.getName());
                }
            }
        };
}