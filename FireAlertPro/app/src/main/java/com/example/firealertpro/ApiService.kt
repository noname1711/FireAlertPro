package com.example.firealertpro

import retrofit2.Call
import retrofit2.http.GET


interface ApiService {
    @GET("data")
    fun getRoomData(): Call<RoomsResponse>
}

