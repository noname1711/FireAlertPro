package com.example.firealertpro

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.SharedPreferences
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button
import android.widget.GridLayout
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private lateinit var sharedPreferences: SharedPreferences
    private lateinit var fireAlertReceiver: BroadcastReceiver
    private lateinit var noFireAlertReceiver: BroadcastReceiver

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Khởi tạo SharedPreferences để lưu trạng thái của các phòng
        sharedPreferences = getSharedPreferences("FireAlertPreferences", MODE_PRIVATE)
        val gridLayout = findViewById<GridLayout>(R.id.gridLayout)
        gridLayout.removeAllViews()

        // Đặt số phòng trên mỗi hàng (5 phòng mỗi hàng)
        gridLayout.columnCount = 5

        // Tạo 10 tầng, mỗi tầng có 10 phòng
        for (floor in 1..10) { // 10 tầng
            for (room in 1..10) { // 10 phòng mỗi tầng
                val roomNumber = if (room == 10) {
                    "P.${floor}10"
                } else {
                    "P.${floor}0${room}"
                }

                val button = Button(this).apply {
                    text = roomNumber
                    layoutParams = GridLayout.LayoutParams().apply {
                        width = GridLayout.LayoutParams.WRAP_CONTENT
                        height = GridLayout.LayoutParams.WRAP_CONTENT
                        setMargins(8, 8, 8, 8)
                    }
                    setOnClickListener {
                        val intent = Intent(this@MainActivity, RoomActivity::class.java)
                        intent.putExtra("roomNumber", roomNumber)
                        startActivity(intent)
                    }
                }
                gridLayout.addView(button)

                // Cập nhật màu cho phòng từ trạng thái đã lưu trong SharedPreferences
                updateRoomColor(roomNumber)
            }
        }

        // Khởi tạo receiver cho thông báo cháy
        fireAlertReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                val roomNumber = intent?.getStringExtra("roomNumber")
                roomNumber?.let {
                    // Lưu trạng thái cháy cho phòng cụ thể
                    val editor = sharedPreferences.edit()
                    editor.putBoolean(it, true) // Lưu trạng thái cháy
                    editor.apply()

                    // Cập nhật màu của phòng bị cháy (màu đỏ)
                    updateRoomColor(it)
                }
            }
        }

        // Đăng ký nhận broadcast thông báo cháy
        val fireAlertFilter = IntentFilter("com.example.firealertpro.FIRE_ALERT")
        registerReceiver(fireAlertReceiver, fireAlertFilter)

        // Khởi tạo receiver cho thông báo hết cháy
        noFireAlertReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                val roomNumber = intent?.getStringExtra("roomNumber")
                roomNumber?.let {
                    // Cập nhật trạng thái không cháy cho phòng
                    val editor = sharedPreferences.edit()
                    editor.putBoolean(it, false) // Lưu trạng thái không cháy
                    editor.apply()

                    // Cập nhật lại màu của phòng khi hết cháy (màu xám)
                    updateRoomColor(it)
                }
            }
        }

        // Đăng ký nhận broadcast thông báo hết cháy
        val noFireAlertFilter = IntentFilter("com.example.firealertpro.NO_FIRE_ALERT")
        registerReceiver(noFireAlertReceiver, noFireAlertFilter)
    }

    // Cập nhật màu của phòng khi nhận được broadcast
    private fun updateRoomColor(roomNumber: String) {
        val gridLayout = findViewById<GridLayout>(R.id.gridLayout)
        for (i in 0 until gridLayout.childCount) {
            val button = gridLayout.getChildAt(i) as Button
            if (button.text == roomNumber) {
                val isFire = sharedPreferences.getBoolean(roomNumber, false)
                if (isFire) {
                    // Phòng cháy -> màu đỏ
                    button.setBackgroundColor(ContextCompat.getColor(this, android.R.color.holo_red_dark))
                } else {
                    // Không cháy -> màu xám
                    button.setBackgroundColor(ContextCompat.getColor(this, android.R.color.darker_gray))
                }
                break
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        // Hủy đăng ký receiver khi Activity bị hủy
        unregisterReceiver(fireAlertReceiver)
        unregisterReceiver(noFireAlertReceiver)
    }
}
