package com.example.firealertpro

class DataModel {
}

data class Reading(
    val temperature: Float,   // Nhiệt độ
    val humidity: Float,      // Độ ẩm
    val gas: Float,           // Khí gas
    val state: Boolean,       // Trạng thái cháy
    val timestamp: Long       // Thời gian
)

data class Room(
    val readings: Map<String, Reading>
)


data class RoomsResponse(
    val rooms: Map<String, Room>
)


