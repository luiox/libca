#include "max30102.h"
#include <em_base/debug.h>

// Register addresses
#define REG_INTR_STATUS_1   0x00
#define REG_INTR_STATUS_2   0x01
#define REG_INTR_ENABLE_1   0x02
#define REG_INTR_ENABLE_2   0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER      0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C
#define REG_LED2_PA         0x0D
#define REG_PILOT_PA        0x10
#define REG_MULTI_LED_CTRL1 0x11
#define REG_MULTI_LED_CTRL2 0x12
#define REG_TEMP_INTR       0x1F
#define REG_TEMP_FRAC       0x20
#define REG_TEMP_CONFIG     0x21
#define REG_PROX_INT_THRESH 0x30
#define REG_REV_ID          0xFE
#define REG_PART_ID         0xFF

#define MAX30102_ADDR 0xAE

// Algorithm constants and buffers
#define FS 100

#define MA4_SIZE  4
#define HAMMING_SIZE  5

static const u16 auw_hamm[31] = { 41, 276, 512, 276, 41 };
static const u8 uch_spo2_table[184] = {
    95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
    99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
    97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
    90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
    80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
    66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
    28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
    3, 2, 1
};

#if (LIBCA_MAX30102_PORT_MODE == LIBCA_MAX30102_PORT_MODE_EXTERN)
static const max30102_port_t* g_port = &g_max30102_port_extern;
#elif (LIBCA_MAX30102_PORT_MODE == LIBCA_MAX30102_PORT_MODE_DYNAMIC)
static const max30102_port_t* g_port = NULL;
#else
#error "Invalid MAX30102 port mode"
#endif

void max30102_bind_port(const max30102_port_t* port)
{
    g_port = port;
}

bool max30102_port_is_registered(void)
{
    return g_port != NULL;
}

static bool max30102_write_reg(max30102_t* self, u8 addr, u8 data)
{
    if (!g_port) {
        debug_print("[max30102] port not registered\n");
        return false;
    }
    i32 ret = g_port->i2c_write(self->hi2c, MAX30102_ADDR, addr, 1, &data, 1, 100);
    return ret == 0;
}

static bool max30102_read_reg(max30102_t* self, u8 addr, u8* data)
{
    if (!g_port) {
        debug_print("[max30102] port not registered\n");
        return false;
    }
    i32 ret = g_port->i2c_read(self->hi2c, MAX30102_ADDR, addr, 1, data, 1, 100);
    return ret == 0;
}

bool max30102_init(max30102_t* self, void* hi2c, i32* dx_buf, usize dx_size, i32* x_buf, usize x_size,
                   i32* y_buf, usize y_size)
{
    if (self == NULL || dx_buf == NULL || x_buf == NULL || y_buf == NULL) {
        return false;
    }
    self->hi2c = hi2c;
    self->an_dx_buf = dx_buf;
    self->an_dx_buf_size = dx_size;
    self->an_x_buf = x_buf;
    self->an_x_buf_size = x_size;
    self->an_y_buf = y_buf;
    self->an_y_buf_size = y_size;

    if (!max30102_write_reg(self, REG_INTR_ENABLE_1, 0xc0)) return false;
    if (!max30102_write_reg(self, REG_INTR_ENABLE_2, 0x00)) return false;
    if (!max30102_write_reg(self, REG_FIFO_WR_PTR, 0x00)) return false;
    if (!max30102_write_reg(self, REG_OVF_COUNTER, 0x00)) return false;
    if (!max30102_write_reg(self, REG_FIFO_RD_PTR, 0x00)) return false;
    if (!max30102_write_reg(self, REG_FIFO_CONFIG, 0x0f)) return false;
    if (!max30102_write_reg(self, REG_MODE_CONFIG, 0x03)) return false;
    if (!max30102_write_reg(self, REG_SPO2_CONFIG, 0x27)) return false;
    if (!max30102_write_reg(self, REG_LED1_PA, 0x24)) return false;
    if (!max30102_write_reg(self, REG_LED2_PA, 0x24)) return false;
    if (!max30102_write_reg(self, REG_PILOT_PA, 0x7f)) return false;

    return true;
}

bool max30102_read_fifo(max30102_t* self, u32* red_led, u32* ir_led)
{
    u8 status;
    u8 data[6];

    if (self == NULL || red_led == NULL || ir_led == NULL) {
        return false;
    }

    if (!max30102_read_reg(self, REG_INTR_STATUS_1, &status)) return false;
    if (!max30102_read_reg(self, REG_INTR_STATUS_2, &status)) return false;

    if (!g_port) {
        return false;
    }

    i32 ret = g_port->i2c_read(self->hi2c, MAX30102_ADDR, REG_FIFO_DATA, 1, data, 6, 100);
    if (ret != 0) return false;

    *red_led = ((u32)data[0] << 16) | ((u32)data[1] << 8) | (u32)data[2];
    *ir_led = ((u32)data[3] << 16) | ((u32)data[4] << 8) | (u32)data[5];

    *red_led &= 0x03FFFF;
    *ir_led &= 0x03FFFF;

    return true;
}

bool max30102_reset(max30102_t* self)
{
    if (self == NULL) {
        return false;
    }
    return max30102_write_reg(self, REG_MODE_CONFIG, 0x40);
}

static void sort_ascend(i32* x, i32 size)
{
    i32 i, j, temp;
    for (i = 1; i < size; i++) {
        temp = x[i];
        for (j = i; j > 0 && temp < x[j - 1]; j--) {
            x[j] = x[j - 1];
        }
        x[j] = temp;
    }
}

static void peaks_above_min_height(i32* locs, i32* npks, i32* x, i32 size, i32 min_height)
{
    i32 i = 1, width;
    *npks = 0;
    while (i < size - 1) {
        if (x[i] > min_height && x[i] > x[i - 1]) {
            width = 1;
            while (i + width < size && x[i] == x[i + width]) width++;
            if (x[i] > x[i + width] && (*npks) < 15) {
                locs[(*npks)++] = i;
                i += width + 1;
            } else i += width;
        } else i++;
    }
}

static void remove_close_peaks(i32* locs, i32* npks, i32* x, i32 min_distance)
{
    i32 i, j, n_dist;
    sort_ascend(locs, *npks);
    for (i = 1; i < *npks; i++) {
        n_dist = locs[i] - locs[i - 1];
        if (n_dist < min_distance) {
            if (x[locs[i]] > x[locs[i - 1]]) {
                for (j = i - 1; j < *npks - 1; j++) locs[j] = locs[j + 1];
                (*npks)--;
                i--;
            } else {
                for (j = i; j < *npks - 1; j++) locs[j] = locs[j + 1];
                (*npks)--;
                i--;
            }
        }
    }
}

static void find_peaks(i32* locs, i32* npks, i32* x, i32 size, i32 min_height, i32 min_distance, i32 max_num)
{
    peaks_above_min_height(locs, npks, x, size, min_height);
    remove_close_peaks(locs, npks, x, min_distance);
    if (*npks > max_num) *npks = max_num;
}

void max30102_calculate(max30102_t* self, u32* ir_buffer, u32* red_buffer, max30102_data_t* result)
{
    u32 ir_mean;
    i32 k, i_ratio_count;
    i32 i, s, m, exact_ir_valley_locs_count, middle_idx;
    i32 th1, npks, c_min;
    i32 ir_valley_locs[15];
    i32 exact_ir_valley_locs[15];
    i32 dx_peak_locs[15];
    i32 peak_interval_sum;
    i32 y_ac, x_ac;
    i32 spo2_calc;
    i32 y_dc_max, x_dc_max;
    i32 y_dc_max_idx = 0, x_dc_max_idx = 0;
    i32 ratio[5], ratio_average;
    i32 nume, denom;

    if (result == NULL || ir_buffer == NULL || red_buffer == NULL || self == NULL) return;
    if (self->an_dx_buf == NULL || self->an_x_buf == NULL || self->an_y_buf == NULL) return;

    ir_mean = 0;
    for (k = 0; k < MAX30102_BUFFER_SIZE; k++) ir_mean += ir_buffer[k];
    ir_mean /= MAX30102_BUFFER_SIZE;
    for (k = 0; k < MAX30102_BUFFER_SIZE; k++) self->an_x_buf[k] = (i32)ir_buffer[k] - (i32)ir_mean;

    for (k = 0; k < MAX30102_BUFFER_SIZE - MA4_SIZE; k++) {
        self->an_x_buf[k] = (self->an_x_buf[k] + self->an_x_buf[k + 1] + self->an_x_buf[k + 2] + self->an_x_buf[k + 3]) / 4;
    }

    for (k = 0; k < MAX30102_BUFFER_SIZE - MA4_SIZE - 1; k++) {
        self->an_dx_buf[k] = self->an_x_buf[k + 1] - self->an_x_buf[k];
    }

    for (k = 0; k < MAX30102_BUFFER_SIZE - MA4_SIZE - 2; k++) {
        self->an_dx_buf[k] = (self->an_dx_buf[k] + self->an_dx_buf[k + 1]) / 2;
    }

    for (i = 0; i < MAX30102_BUFFER_SIZE - HAMMING_SIZE - MA4_SIZE - 2; i++) {
        s = 0;
        for (k = i; k < i + HAMMING_SIZE; k++) {
            s -= self->an_dx_buf[k] * auw_hamm[k - i];
        }
        self->an_dx_buf[i] = s / 1146;
    }

    th1 = 0;
    for (k = 0; k < MAX30102_BUFFER_SIZE - HAMMING_SIZE; k++) {
        th1 += (self->an_dx_buf[k] > 0) ? self->an_dx_buf[k] : -self->an_dx_buf[k];
    }
    th1 /= (MAX30102_BUFFER_SIZE - HAMMING_SIZE);

    find_peaks(dx_peak_locs, &npks, self->an_dx_buf, MAX30102_BUFFER_SIZE - HAMMING_SIZE, th1, 8, 5);

    peak_interval_sum = 0;
    if (npks >= 2) {
        for (k = 1; k < npks; k++) peak_interval_sum += (dx_peak_locs[k] - dx_peak_locs[k - 1]);
        peak_interval_sum /= (npks - 1);
        result->heart_rate = 6000 / peak_interval_sum;
        result->heart_rate_valid = true;
    } else {
        result->heart_rate = -999;
        result->heart_rate_valid = false;
    }

    for (k = 0; k < npks; k++) ir_valley_locs[k] = dx_peak_locs[k] + HAMMING_SIZE / 2;

    for (k = 0; k < MAX30102_BUFFER_SIZE; k++) {
        self->an_x_buf[k] = (i32)ir_buffer[k];
        self->an_y_buf[k] = (i32)red_buffer[k];
    }

    exact_ir_valley_locs_count = 0;
    for (k = 0; k < npks; k++) {
        bool only_once = true;
        m = ir_valley_locs[k];
        c_min = 16777216;
        if (m + 5 < MAX30102_BUFFER_SIZE - HAMMING_SIZE && m - 5 > 0) {
            for (i = m - 5; i < m + 5; i++) {
                if (self->an_x_buf[i] < c_min) {
                    only_once = false;
                    c_min = self->an_x_buf[i];
                    exact_ir_valley_locs[k] = i;
                }
            }
            if (!only_once) exact_ir_valley_locs_count++;
        }
    }

    if (exact_ir_valley_locs_count < 2) {
        result->spo2 = -999;
        result->spo2_valid = false;
        return;
    }

    for (k = 0; k < MAX30102_BUFFER_SIZE - MA4_SIZE; k++) {
        self->an_x_buf[k] = (self->an_x_buf[k] + self->an_x_buf[k + 1] + self->an_x_buf[k + 2] + self->an_x_buf[k + 3]) / 4;
        self->an_y_buf[k] = (self->an_y_buf[k] + self->an_y_buf[k + 1] + self->an_y_buf[k + 2] + self->an_y_buf[k + 3]) / 4;
    }

    ratio_average = 0;
    i_ratio_count = 0;
    for (k = 0; k < 5; k++) ratio[k] = 0;

    for (k = 0; k < exact_ir_valley_locs_count - 1; k++) {
        y_dc_max = -16777216;
        x_dc_max = -16777216;
        if (exact_ir_valley_locs[k + 1] - exact_ir_valley_locs[k] > 10) {
            for (i = exact_ir_valley_locs[k]; i < exact_ir_valley_locs[k + 1]; i++) {
                if (self->an_x_buf[i] > x_dc_max) { x_dc_max = self->an_x_buf[i]; x_dc_max_idx = i; }
                if (self->an_y_buf[i] > y_dc_max) { y_dc_max = self->an_y_buf[i]; y_dc_max_idx = i; }
            }
            y_ac = (self->an_y_buf[exact_ir_valley_locs[k + 1]] - self->an_y_buf[exact_ir_valley_locs[k]]) * (y_dc_max_idx - exact_ir_valley_locs[k]);
            y_ac = self->an_y_buf[exact_ir_valley_locs[k]] + y_ac / (exact_ir_valley_locs[k + 1] - exact_ir_valley_locs[k]);
            y_ac = self->an_y_buf[y_dc_max_idx] - y_ac;

            x_ac = (self->an_x_buf[exact_ir_valley_locs[k + 1]] - self->an_x_buf[exact_ir_valley_locs[k]]) * (x_dc_max_idx - exact_ir_valley_locs[k]);
            x_ac = self->an_x_buf[exact_ir_valley_locs[k]] + x_ac / (exact_ir_valley_locs[k + 1] - exact_ir_valley_locs[k]);
            x_ac = self->an_x_buf[y_dc_max_idx] - x_ac;

            nume = (y_ac * x_dc_max) >> 7;
            denom = (x_ac * y_dc_max) >> 7;
            if (denom > 0 && i_ratio_count < 5 && nume != 0) {
                ratio[i_ratio_count] = (nume * 100) / denom;
                i_ratio_count++;
            }
        }
    }

    sort_ascend(ratio, i_ratio_count);
    middle_idx = i_ratio_count / 2;

    if (middle_idx > 1) ratio_average = (ratio[middle_idx - 1] + ratio[middle_idx]) / 2;
    else ratio_average = ratio[middle_idx];

    if (ratio_average > 2 && ratio_average < 184) {
        spo2_calc = uch_spo2_table[ratio_average];
        result->spo2 = spo2_calc;
        result->spo2_valid = true;
    } else {
        result->spo2 = -999;
        result->spo2_valid = false;
    }
}
