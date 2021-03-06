/*
 * Enlighted-Otter  -  Stm32f334 based mobile worklight.
 * Copyright (C) 2018 Jan Henrik Hemsing
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of  MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"
#include "ntc.h"
#include "gamma.h"

void enable_OTG(void) {
	configure_RT(CHG_CTRL16, DISABLE_UUG);
	configure_RT(CHG_CTRL1, ENABLE_OTG_MASK);
}

void disable_OTG(void) {
	configure_RT(CHG_CTRL16, ENABLE_UUG);
	configure_RT(CHG_CTRL1, DISABLE_OTG_MASK);
}

uint16_t read_RT_ADC(void) {
	uint16_t _cnt = 0;
	uint8_t _ADC_H, _ADC_L;
	uint8_t _tmp_data_H = ADC_DATA_H;
	uint8_t _tmp_data_L = ADC_DATA_L;

	while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) if (_cnt++ > 10000) break;
	HAL_I2C_Master_Transmit_DMA(&hi2c1, RT_ADDRESS, &_tmp_data_H, 1);
	_cnt = 0;
	while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) if (_cnt++ > 10000) break;
	HAL_I2C_Master_Receive_DMA(&hi2c1, RT_ADDRESS, &_ADC_H, 1);
	_cnt = 0;
	while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) if (_cnt++ > 10000) break;
	HAL_I2C_Master_Transmit_DMA(&hi2c1, RT_ADDRESS, &_tmp_data_L, 1);
	_cnt = 0;
	while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) if (_cnt++ > 10000) break;
	HAL_I2C_Master_Receive_DMA(&hi2c1, RT_ADDRESS, &_ADC_L, 1);
	_cnt = 0;
	while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) if (_cnt++ > 10000) break;

	uint16_t _tmp_data = ((_ADC_H << 8) | (_ADC_L & 0xFF));
	return _tmp_data;
}

uint8_t read_RT_status(uint8_t _mask){
	uint8_t ret;
	uint8_t _cnt = 0;
	uint8_t _register = CHG_STAT;

	while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) if (_cnt++ > 10000) break;
	HAL_I2C_Master_Transmit_DMA(&hi2c1, RT_ADDRESS, &_register, 1);
	_cnt = 0;
    while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) if (_cnt++ > 10000) break;
	HAL_I2C_Master_Receive_DMA(&hi2c1, RT_ADDRESS, &ret, 1);
	_cnt = 0;
	while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) if (_cnt++ > 10000) break;
	ret = ret & _mask;
	return ret;
}

void set_pwm(uint8_t timer, float duty) {

	/* Clamp duty cycle values */
	if (duty < MIN_DUTY) duty = MIN_DUTY;
	if (duty > MAX_DUTY) duty = MAX_DUTY;

	/* Set registers according to duty cycle */
	HRTIM1->sTimerxRegs[timer].CMP1xR = HRTIM_PERIOD * duty;
	HRTIM1->sTimerxRegs[timer].CMP2xR = HRTIM_PERIOD - (HRTIM_PERIOD * duty);
	HRTIM1->sTimerxRegs[timer].SETx1R = HRTIM_SET1R_PER;
	HRTIM1->sTimerxRegs[timer].RSTx1R = HRTIM_RST1R_CMP1;
	HRTIM1->sTimerxRegs[timer].SETx2R = HRTIM_SET2R_CMP2;
	HRTIM1->sTimerxRegs[timer].RSTx2R = HRTIM_RST2R_PER;
}

float ntc_calc(uint16_t adc_value) {

	int16_t p1, p2;
	p1 = NTC_table[(adc_value >> 7)];
	p2 = NTC_table[(adc_value >> 7) + 1];

	return (p1 - ( (p1 - p2) * (adc_value & 0x007F) ) / 128.0f ) / 2.0f;
}

float gamma_calc(float target){
	float p1,p2;
	
	p1 = gammaTable[(int)target];
	p2 = gammaTable[(int)target+1];

	return p1 + ((p1-p2)*((int)target-target));
}


#if defined(SCOPE_CHANNELS)
void set_scope_channel(uint8_t ch, int16_t val) {
	ch_buf[ch] = val;
}

void console_scope(void) {
	memset(uart_buf, 0, sizeof(uart_buf));

#if (SCOPE_CHANNELS == 1)
	sprintf((char*)uart_buf, "%i\n\r", ch_buf[0]);
#elif (SCOPE_CHANNELS == 2)
	sprintf((char*)uart_buf, "%i\t%i\n\r", ch_buf[0], ch_buf[1]);
#elif (SCOPE_CHANNELS == 3)
	sprintf((char*)uart_buf, "%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2]);
#elif (SCOPE_CHANNELS == 4)
	sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3]);
#elif (SCOPE_CHANNELS == 5)
	sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4]);
#elif (SCOPE_CHANNELS == 6)
	sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4], ch_buf[5]);
#elif (SCOPE_CHANNELS == 7)
	sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4], ch_buf[5], ch_buf[6]);
#elif (SCOPE_CHANNELS == 8)
	sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4], ch_buf[5], ch_buf[6], ch_buf[7]);
#endif

	HAL_UART_Transmit_DMA(&huart1, (uint8_t *)uart_buf, strlen((char*)uart_buf));
	huart1.gState = HAL_UART_STATE_READY;
}
#endif
