package com.example.firealertpro

import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.github.mikephil.charting.charts.LineChart
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.data.Entry
import com.github.mikephil.charting.data.LineData
import com.github.mikephil.charting.data.LineDataSet
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import retrofit2.Call
import retrofit2.Callback
import retrofit2.Response
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import android.util.Log
import androidx.core.app.NotificationCompat
import com.github.mikephil.charting.formatter.ValueFormatter
import java.text.SimpleDateFormat
import java.util.*

class RoomActivity : AppCompatActivity() {

    private lateinit var temperatureChart: LineChart
    private lateinit var humidityChart: LineChart
    private lateinit var gasChart: LineChart
    private lateinit var tvFireState: TextView
    private lateinit var roomNumber: String
    private lateinit var apiService: ApiService
    private val NOTIFICATION_CHANNEL_ID = "FIRE_ALERT_CHANNEL"
    private val NOTIFICATION_ID = 1
    private lateinit var tvAverageTemperature: TextView
    private lateinit var tvAverageHumidity: TextView
    private lateinit var tvAverageGas: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_room)

        tvAverageTemperature = findViewById(R.id.tvAverageTemperature)
        tvAverageHumidity = findViewById(R.id.tvAverageHumidity)
        tvAverageGas = findViewById(R.id.tvAverageGas)

        // Get room number from Intent
        roomNumber = intent.getStringExtra("roomNumber") ?: "Unknown Room"
        val tvRoomNumber: TextView = findViewById(R.id.tvRoomNumber)
        tvRoomNumber.text = roomNumber

        // Initialize charts and TextView
        temperatureChart = findViewById(R.id.temperatureChart)
        humidityChart = findViewById(R.id.humidityChart)
        gasChart = findViewById(R.id.gasChart)
        tvFireState = findViewById(R.id.tvFireState)

        // Configure Retrofit
        val retrofit = Retrofit.Builder()
            .baseUrl("http://45.117.179.18:5000/")
            .addConverterFactory(GsonConverterFactory.create())
            .build()

        apiService = retrofit.create(ApiService::class.java)

        // Setup charts
        setupChart(temperatureChart, "Nhiệt độ (°C)")
        setupChart(humidityChart, "Độ ẩm (%)")
        setupChart(gasChart, "Khí gas (ppm)")

        // Fetch data from API
        fetchDataFromApi()

        // Initialize notification channel
        createNotificationChannel()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val name = "Cảnh báo cháy"
            val descriptionText = "Thông báo khi phát hiện cháy"
            val importance = NotificationManager.IMPORTANCE_HIGH
            val channel = NotificationChannel(NOTIFICATION_CHANNEL_ID, name, importance).apply {
                description = descriptionText
            }
            val notificationManager: NotificationManager =
                getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            notificationManager.createNotificationChannel(channel)
        }
    }

    private fun setupChart(chart: LineChart, label: String) {
        chart.description.isEnabled = false
        chart.xAxis.position = XAxis.XAxisPosition.BOTTOM
        chart.xAxis.setDrawGridLines(false)
        chart.axisLeft.setDrawGridLines(false)
        chart.axisRight.isEnabled = false
        chart.legend.isEnabled = true
        chart.description.text = label

        // Định dạng trục X để hiển thị thời gian thực
        chart.xAxis.valueFormatter = object : ValueFormatter() {
            override fun getFormattedValue(value: Float): String {
                val sdf = SimpleDateFormat("HH:mm:ss", Locale.getDefault()) // Định dạng thời gian
                return sdf.format(Date(value.toLong())) // Chuyển đổi giá trị X (timestamp) thành thời gian
            }
        }
    }


    private fun fetchDataFromApi() {
        apiService.getRoomData().enqueue(object : Callback<RoomsResponse> {
            override fun onResponse(call: Call<RoomsResponse>, response: Response<RoomsResponse>) {
                if (response.isSuccessful) {
                    // Lấy danh sách các phòng từ response
                    val rooms = response.body()?.rooms ?: emptyMap()
                    val room = rooms[roomNumber] // Lấy phòng theo roomNumber

                    room?.let {
                        // Truy xuất các giá trị readings từ phòng
                        val readings = it.readings.values.toList() // Lấy tất cả readings của phòng
                        updateChartsAndState(readings) // Cập nhật biểu đồ và trạng thái
                    } ?: run {
                        // Xử lý nếu không có phòng dữ liệu cho roomNumber
                        handleNoData()
                    }
                } else {
                    Log.e("API Error", "Error: ${response.code()} - ${response.message()}")
                    handleNoData() // Xử lý khi không có dữ liệu
                }
            }

            override fun onFailure(call: Call<RoomsResponse>, t: Throwable) {
                // In ra lỗi chi tiết
                Log.e("API Failure", "Failure: ${t.message}")
                tvFireState.text = "Không thể kết nối với server!"
                tvFireState.setTextColor(ContextCompat.getColor(this@RoomActivity, android.R.color.holo_red_light))
                temperatureChart.visibility = View.GONE
                humidityChart.visibility = View.GONE
                gasChart.visibility = View.GONE
            }
        })

    }


    private fun handleNoData() {
        tvFireState.text = "Không có dữ liệu!"
        tvFireState.setTextColor(ContextCompat.getColor(this, android.R.color.holo_red_light))
        temperatureChart.visibility = View.GONE
        humidityChart.visibility = View.GONE
        gasChart.visibility = View.GONE
    }

    private fun updateChartsAndState(readings: List<Reading>) {
        // Sắp xếp readings theo timestamp từ nhỏ đến lớn
        val sortedReadings = readings.sortedBy { it.timestamp }

        val temperatureEntries = mutableListOf<Entry>()
        val humidityEntries = mutableListOf<Entry>()
        val gasEntries = mutableListOf<Entry>()
        var latestState: Boolean? = null

        val sdf = SimpleDateFormat("HH:mm:ss", Locale.getDefault()) // Chuyển đổi thời gian theo giờ:phút:giây

        sortedReadings.forEachIndexed { index, reading ->
            // Tạo các Entry cho biểu đồ với timestamp là giá trị X
            val timestamp = reading.timestamp
            val formattedTime = sdf.format(Date(timestamp)) // Chuyển đổi timestamp thành thời gian thực

            temperatureEntries.add(Entry(timestamp.toFloat(), reading.temperature))
            humidityEntries.add(Entry(timestamp.toFloat(), reading.humidity))
            gasEntries.add(Entry(timestamp.toFloat(), reading.gas))
            latestState = reading.state

            // In ra thời gian đã định dạng
            Log.d("RoomActivity", "Timestamp: $formattedTime, Temperature: ${reading.temperature}, Humidity: ${reading.humidity}, Gas: ${reading.gas}")
        }
        calculateAndDisplayAverages(readings)
        // Cập nhật biểu đồ
        updateChart(temperatureChart, temperatureEntries, "Nhiệt độ")
        updateChart(humidityChart, humidityEntries, "Độ ẩm")
        updateChart(gasChart, gasEntries, "Khí gas")

        // Cập nhật trạng thái cháy
        updateFireState(latestState)

        // Hiển thị các biểu đồ
        temperatureChart.visibility = View.VISIBLE
        humidityChart.visibility = View.VISIBLE
        gasChart.visibility = View.VISIBLE

        //tính tb
        calculateAndDisplayAverages(readings)
    }



    private fun updateChart(chart: LineChart, entries: List<Entry>, label: String) {
        val dataSet = LineDataSet(entries, label).apply {
            color = ContextCompat.getColor(this@RoomActivity, android.R.color.holo_red_light)
            valueTextColor = ContextCompat.getColor(this@RoomActivity, android.R.color.black)
            lineWidth = 2f
            circleRadius = 4f
            setDrawValues(false)
        }
        chart.data = LineData(dataSet)  
        chart.invalidate() // Refresh chart
    }

    private fun updateFireState(latestState: Boolean?) {
        if (latestState == true) {
            tvFireState.text = "Có cháy!"
            tvFireState.setTextColor(ContextCompat.getColor(this, android.R.color.holo_red_dark))
            // Gửi thông báo khi phát hiện cháy
            sendFireNotification()
        } else {
            tvFireState.text = "Không có cháy"
            tvFireState.setTextColor(ContextCompat.getColor(this, android.R.color.black))
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

    private fun calculateAndDisplayAverages(readings: List<Reading>) {
        if (readings.isNotEmpty()) {
            val averageTemperature = readings.map { it.temperature }.average()
            val averageHumidity = readings.map { it.humidity }.average()
            val averageGas = readings.map { it.gas }.average()

            tvAverageTemperature.text = "Nhiệt độ trung bình: %.2f °C".format(averageTemperature)
            tvAverageHumidity.text = "Độ ẩm trung bình: %.2f %%".format(averageHumidity)
            tvAverageGas.text = "Nồng độ gas trung bình: %.2f ppm".format(averageGas)
        } else {
            tvAverageTemperature.text = "Nhiệt độ trung bình: N/A"
            tvAverageHumidity.text = "Độ ẩm trung bình: N/A"
            tvAverageGas.text = "Nồng độ gas trung bình: N/A"
        }
    }
}