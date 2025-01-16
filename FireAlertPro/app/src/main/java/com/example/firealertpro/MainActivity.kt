package com.example.firealertpro

import android.app.NotificationManager
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.GridLayout
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import retrofit2.Call
import retrofit2.Callback
import retrofit2.Response
import android.util.Log
import androidx.core.app.NotificationCompat

class MainActivity : AppCompatActivity() {

    private lateinit var apiService: ApiService
    private lateinit var gridLayout: GridLayout
    private val NOTIFICATION_CHANNEL_ID = "FIRE_ALERT_CHANNEL"
    private val NOTIFICATION_ID = 1

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        gridLayout = findViewById(R.id.gridLayout)
        gridLayout.removeAllViews()
        gridLayout.columnCount = 5

        // Tạo Retrofit instance
        val retrofit = Retrofit.Builder()
            .baseUrl("http://45.117.179.18:5000/")
            .addConverterFactory(GsonConverterFactory.create())
            .build()
        apiService = retrofit.create(ApiService::class.java)

        // Tạo giao diện các phòng
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
            }
        }

        // Fetch trạng thái phòng từ API
        fetchRoomStates()
    }

    private fun fetchRoomStates() {
        apiService.getRoomData().enqueue(object : Callback<RoomsResponse> {
            override fun onResponse(call: Call<RoomsResponse>, response: Response<RoomsResponse>) {
                if (response.isSuccessful) {
                    val rooms = response.body()?.rooms ?: emptyMap()
                    updateRoomColors(rooms)
                    //check từng phòng xem có phg nào cháy ko
                    if (rooms.values.any { room -> room.readings.values.any { it.state } }) {
                        sendFireNotification()
                    }
                } else {
                    Log.e("API Error", "Error: ${response.code()} - ${response.message()}")
                }
            }

            override fun onFailure(call: Call<RoomsResponse>, t: Throwable) {
                Log.e("API Failure", "Failure: ${t.message}")
                t.printStackTrace()
            }
        })
    }

    private fun updateRoomColors(rooms: Map<String, Room>) {
        for (i in 0 until gridLayout.childCount) {
            val button = gridLayout.getChildAt(i) as Button
            val roomNumber = button.text.toString()
            val room = rooms[roomNumber]

            room?.let {
                val isFire = it.readings.values.any { reading -> reading.state }
                val color = if (isFire) {
                    ContextCompat.getColor(this, android.R.color.holo_red_dark)
                } else {
                    ContextCompat.getColor(this, android.R.color.darker_gray)
                }
                button.setBackgroundColor(color)
            }
        }
    }

    private fun sendFireNotification() {
        // Tạo thông báo
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val notification = NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setSmallIcon(R.drawable.fire)
            .setContentTitle("Cảnh báo cháy!")
            .setContentText("Có cháy, vui lòng sơ tán và gọi cứu hộ!")
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setAutoCancel(true)
            .build()
        notificationManager.notify(NOTIFICATION_ID, notification)
    }
}