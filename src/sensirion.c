//
//  sensirion.c
//  sps30
//
//  Created by Benjamin Rannow on 02.02.19.
//  Copyright © 2019 Fuyukai Rannow. All rights reserved.
//

#include <stdint.h>
#include <unistd.h>

#include "sensirion.h"
#include "i2c_hw.h"

int8_t sensirion_init(const char *path, uint8_t address)
{
    if (i2c_init(path) != -1 && i2c_set_address(address) != -1) {
        return 0;
    }
    
    return -1;
}

uint8_t sensirion_common_generate_crc(uint8_t *data, uint16_t count)
{
    uint16_t current_byte;
    uint8_t crc = 0xFF;
    uint8_t crc_bit;
    
    /* calculates 8-Bit checksum with given polynomial */
    for (current_byte = 0; current_byte < count; ++current_byte) {
        crc ^= (data[current_byte]);
        for (crc_bit = 8; crc_bit > 0; --crc_bit) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

int8_t sensirion_check_crc(uint8_t *data, uint16_t count, uint8_t checksum)
{
    if (sensirion_common_generate_crc(data, count) != checksum)
        return STATUS_FAIL;
    return STATUS_OK;
}

uint16_t sensirion_fill_cmd_send_buf(uint8_t *buf, uint16_t cmd, const uint16_t *args, uint8_t num_args)
{
    uint8_t crc;
    uint8_t i;
    uint16_t idx = 0;
    
    buf[idx++] = (uint8_t)((cmd & 0xFF00) >> 8);
    buf[idx++] = (uint8_t)((cmd & 0x00FF) >> 0);
    
    for (i = 0; i < num_args; ++i) {
        buf[idx++] = (uint8_t)((args[i] & 0xFF00) >> 8);
        buf[idx++] = (uint8_t)((args[i] & 0x00FF) >> 0);
        
        crc = sensirion_common_generate_crc((uint8_t *)&buf[idx - 2],
                                            SENSIRION_WORD_SIZE);
        buf[idx++] = crc;
    }
    return idx;
}

int16_t sensirion_read_bytes(uint8_t *data, uint16_t num_words)
{
    int16_t ret;
    uint16_t i, j;
    uint16_t size = num_words * (SENSIRION_WORD_SIZE + CRC8_LEN);
    uint16_t word_buf[SENSIRION_MAX_BUFFER_WORDS];
    uint8_t * const buf8 = (uint8_t *)word_buf;
    
    ret = i2c_read(buf8, size);
    if (ret != STATUS_OK)
        return ret;
    
    /* check the CRC for each word */
    for (i = 0, j = 0; i < size; i += SENSIRION_WORD_SIZE + CRC8_LEN) {
        
        ret = sensirion_check_crc(&buf8[i], SENSIRION_WORD_SIZE, buf8[i + SENSIRION_WORD_SIZE]);
        if (ret != STATUS_OK)
            return ret;
        
        data[j++] = buf8[i];
        data[j++] = buf8[i + 1];
    }
    
    return STATUS_OK;
}

int16_t sensirion_read_words(uint16_t *data_words, uint16_t num_words)
{
    int16_t ret;
    uint8_t i;
    
    ret = sensirion_read_bytes((uint8_t *)data_words, num_words);
    if (ret != STATUS_OK)
        return ret;
    
    for (i = 0; i < num_words; ++i)
        data_words[i] = be16_to_cpu(data_words[i]);
    
    return STATUS_OK;
}

int16_t sensirion_write_cmd(uint16_t command)
{
    uint8_t buf[SENSIRION_COMMAND_SIZE];
    
    sensirion_fill_cmd_send_buf(buf, command, NULL, 0);
    return i2c_write(buf, SENSIRION_COMMAND_SIZE);
}

int16_t sensirion_write_cmd_with_args(uint16_t command, const uint16_t *data_words, uint16_t num_words)
{
    uint8_t buf[SENSIRION_MAX_BUFFER_WORDS];
    uint16_t buf_size;
    
    buf_size = sensirion_fill_cmd_send_buf(buf, command, data_words, num_words);
    return i2c_write(buf, buf_size);
}

int16_t sensirion_read_delayed_cmd(uint16_t cmd, uint32_t delay_us, uint16_t *data_words, uint16_t num_words)
{
    int16_t ret;
    uint8_t buf[SENSIRION_COMMAND_SIZE];
    
    sensirion_fill_cmd_send_buf(buf, cmd, NULL, 0);
    ret = i2c_write(buf, SENSIRION_COMMAND_SIZE);
    if (ret != STATUS_OK)
        return ret;
    
    if (delay_us)
        usleep(delay_us);
    
    return sensirion_read_words(data_words, num_words);
}

int16_t sensirion_read_cmd(uint16_t cmd, uint16_t *data_words, uint16_t num_words)
{
    return sensirion_read_delayed_cmd(cmd, 0, data_words, num_words);
}
