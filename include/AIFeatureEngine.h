#ifndef FEATURE_ENGINE_H
#define FEATURE_ENGINE_H

#include <Arduino.h>
#include <math.h>
#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; // GMT+7 cho Việt Nam
const int   daylightOffset_sec = 0;   // Việt Nam không có giờ mùa hè

void setupTime() {
    // Cấu hình thời gian
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

class FeatureEngine {
public:
    // IMPORTANT: These values must be updated from your Python 'norm_params.h'
    float min_vals[11] = { -0.166491f, -1.000000f, 0.000000f, 32.690000f, 43.210000f, -0.390000f, -3.040000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f };
    float max_vals[11] = { 0.215581f, -0.976486f, 0.000000f, 42.630000f, 85.120000f, 0.830000f, 3.320000f, 0.579886f, 2.210134f, 1.479370f, 5.652954f };

    FeatureEngine() {
        memset(t_raw_buf, 0, sizeof(t_raw_buf));
        memset(h_raw_buf, 0, sizeof(h_raw_buf));
        memset(input_tensor, 0, sizeof(input_tensor));
    }

    /**
     * @brief Preprocessing pipeline with internal time retrieval.
     * @param raw_t Raw temperature from sensor.
     * @param raw_h Raw humidity from sensor.
     * @return Pointer to 220-element flattened array for 1D CNN.
     */
    float* get_ai_input(float raw_t, float raw_h) {
        // 1. Internal high-precision time retrieval
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            // If NTP is not synced yet, default to midnight
            memset(&timeinfo, 0, sizeof(timeinfo));
        }
        uint32_t us = micros() % 1000000;

        // Cyclical Time calculation matching Python logic
        float current_hour_float = (float)timeinfo.tm_hour + 
                                   (float)timeinfo.tm_min / 60.0f + 
                                   (float)timeinfo.tm_sec / 3600.0f + 
                                   (float)us / 3600000000.0f;
        
        float h_sin = sin(2.0f * PI * current_hour_float / 24.0f);
        float h_cos = cos(2.0f * PI * current_hour_float / 24.0f);

        // 2. Update rolling buffers (Window size 10)
        update_raw_buffers(raw_t, raw_h);

        // 3. Feature extraction (11 columns)
        float current_f[11];
        current_f[0] = h_sin;
        current_f[1] = h_cos;
        current_f[2] = 0.0f; // location_id

        // Denoising & Dynamics
        current_f[3] = get_median_3(t_raw_buf[9], t_raw_buf[8], t_raw_buf[7]);
        current_f[4] = get_median_3(h_raw_buf[9], h_raw_buf[8], h_raw_buf[7]);
        current_f[5] = current_f[3] - last_smooth_t;
        current_f[6] = current_f[4] - last_smooth_h;
        
        last_smooth_t = current_f[3];
        last_smooth_h = current_f[4];

        // Stats & Spectral Energy
        current_f[7] = calculate_std_dev(t_raw_buf, 10);
        current_f[8] = calculate_std_dev(h_raw_buf, 10);
        current_f[9] = calculate_dft_energy(t_raw_buf, 10);
        current_f[10] = calculate_dft_energy(h_raw_buf, 10);

        Serial.printf("Temperature before normalization: %.4f\n", current_f[3]);
        Serial.printf("Humidity before normalization: %.4f\n", current_f[4]);

        // 4. MinMaxScaler Normalization
        for (int i = 0; i < 11; i++) {
            float range = max_vals[i] - min_vals[i];
            if (range > 0.000001f) { // Kiểm tra nếu range khác 0
                current_f[i] = (current_f[i] - min_vals[i]) / range;
            } else {
                current_f[i] = 0.0f; // Nếu max == min, mặc định gán bằng 0
            }
            current_f[i] = constrain(current_f[i], 0.0f, 1.0f);
            Serial.printf("Feature %d: %.4f\n", i, current_f[i]);
        }

        // 5. Sliding window update (20 frames x 11 features)
        memmove(input_tensor, input_tensor + 11, 19 * 11 * sizeof(float));
        memcpy(input_tensor + (19 * 11), current_f, 11 * sizeof(float));

        return input_tensor;
    }

private:
    float t_raw_buf[10], h_raw_buf[10];
    float input_tensor[220];
    float last_smooth_t = 0, last_smooth_h = 0;

    void update_raw_buffers(float t, float h) {
        memmove(t_raw_buf, t_raw_buf + 1, 9 * sizeof(float));
        t_raw_buf[9] = t;
        memmove(h_raw_buf, h_raw_buf + 1, 9 * sizeof(float));
        h_raw_buf[9] = h;
    }

    float get_median_3(float a, float b, float c) {
        if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
        if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
        return c;
    }

    float calculate_std_dev(float* data, int n) {
        float sum = 0, mean = 0, var = 0;
        for (int i = 0; i < n; i++) sum += data[i];
        mean = sum / n;
        for (int i = 0; i < n; i++) var += pow(data[i] - mean, 2);
        return sqrt(var / (n - 1));
    }

    float calculate_dft_energy(float* data, int n) {
        float energy = 0;
        for (int k = 1; k < n; k++) {
            float re = 0, im = 0;
            for (int t = 0; t < n; t++) {
                float angle = 2 * PI * k * t / n;
                re += data[t] * cos(angle);
                im -= data[t] * sin(angle);
            }
            energy += sqrt(re*re + im*im);
        }
        return energy / n;
    }
};

#endif