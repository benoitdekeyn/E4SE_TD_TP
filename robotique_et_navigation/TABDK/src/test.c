if (HAL_I2C_Mem_Read(&hi2c1,
                           TMP102_ADDR,
                           TMP102_TEMP_REG,
                           I2C_MEMADD_SIZE_8BIT,
                           temp_data,
                           2,
                           100) == HAL_OK)
      {
        temp_value =
            ((uint16_t)temp_data[0] << 8) |
            temp_data[1];

        temp_raw =
            (int16_t)(temp_value >> 4);

        if (temp_raw & 0x0800)
        {
          temp_raw |= 0xF000;
        }

        temperature_x100 =
            ((int32_t)temp_raw * 625) / 100;
      }