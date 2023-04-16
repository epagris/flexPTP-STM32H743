/*
 * tlv320.c
 *
 *  Created on: 2021. okt. 24.
 *      Author: epagris
 */

#include "tlv320.h"
#include <string.h>

static I2C_HandleTypeDef * sphI2C; // handle pointer for I2C peripheral TLV320 is connected to
static TLV320_State sState; // state
static uint8_t spBuf[2]; // transmit buffer
static bool sFullUpdate = false; // do a full update

#define TLV320_REGARR_SIZE (9) // register array size NOT INCLUDING activation and reset register
static uint16_t sRS[TLV320_REGARR_SIZE], sRS_prev[TLV320_REGARR_SIZE]; // register state and previous register state


static void TLV320_write(uint8_t addr, uint16_t value) {
	spBuf[0] = (uint8_t)((addr << 1) | ((value >> 8) & 1));
	spBuf[1] = (uint8_t)(value & 0xFF);
	HAL_I2C_Master_Transmit(sphI2C, sState.i2c_addr, spBuf, 2, 1000);
}

void TLV320_init(I2C_HandleTypeDef * phI2C, uint8_t i2c_addr) {
	sphI2C = phI2C;
	sState.i2c_addr = i2c_addr;

	TLV320_reset();
}

void TLV320_updateOptions(TLV320_Options * pOpt) {
	// save previous state
	memcpy(sRS_prev, sRS, TLV320_REGARR_SIZE * sizeof(uint16_t));

	// fill in bitfields
	/* LEFT LINE INPUT CHANNEL */
	sRS[0] = (pOpt->LRS << 8) | (pOpt->LIM << 7) | (pOpt->LIV & 0b11111);

	/* RIGHT LINE INPUT CHANNEL */
	sRS[1] = (pOpt->RLS << 8) | (pOpt->RIM << 7) | (pOpt->RIV & 0b11111);

	/* LEFT HEADPHONE CHANNEL */
	sRS[2] = (pOpt->LRS_HP << 8) | (pOpt->LZC << 7) | (pOpt->LHV & 0b1111111);

	/* RIGHT HEADPHONE CHANNEL */
	sRS[3] = (pOpt->RLS_HP << 8) | (pOpt->RZC << 7) | (pOpt->RHV & 0b1111111);

	/* ANALOG AUDIO PATH CONTROL */
	sRS[4] = ((pOpt->STA_STE & 0b1111) << 5) | (pOpt->DAC_SEL << 4) | (pOpt->BYP << 3) | (pOpt->INSEL << 2) | (pOpt->MICM << 1) | (pOpt->MICB);

	/* DIGITAL AUDIO PATH CONTROL */
	sRS[5] = (pOpt->DACM << 3) | ((pOpt->DEEMP & 0b11) << 1) | (pOpt->ADCHP) | 0;

	/* POWER DOWN CONTROL */
	sRS[6] = (pOpt->PD_OFF << 7) | (pOpt->PD_CLK << 6) | (pOpt->PD_OSC << 5) |
			(pOpt->PD_OUT << 4) | (pOpt->PD_DAC << 3) | (pOpt->PD_ADC << 2) |
			(pOpt->PD_MIC << 1) | (pOpt->PD_LINE);

	/* DIGITAL AUDIO INTERFACE FORMAT */
	sRS[7] = (pOpt->MS << 6) | (pOpt->LRSWAP << 5) | (pOpt->LRP << 4) | ((pOpt->IWL & 0b11) << 2) | ((pOpt->FOR) & 0b11);

	/* SAMPLE RATE CONTROL */
	sRS[8] = (pOpt->CLKOUT << 7) | (pOpt->CLKIN << 6) | ((pOpt->SR & 0b1111) << 2) | (pOpt->BOSR << 1) | (pOpt->USB_Normal);

	// compare old and new state
	uint8_t i;
	for (i = 0; i < TLV320_REGARR_SIZE; i++) {
		if (sRS[i] != sRS_prev[i] || sFullUpdate) { // if register content has changed...
			TLV320_write(i, sRS[i]); // ...then write to device
		}
	}

	// save new settings
	if (pOpt != &sState.options) {
		sState.options = *pOpt;
	}

	// full update done
	sFullUpdate = false;
}

void TLV320_reset() {
	TLV320_Options * pOpt = &sState.options;
	memcpy(sRS_prev, sRS, TLV320_REGARR_SIZE * sizeof(uint16_t));

	// reset options

	/* LEFT LINE INPUT */
	pOpt->LRS = 0;
	pOpt->LIM = 1;
	pOpt->LIV = 0b10111;

	/* RIGHT LINE INPUT */
	pOpt->RLS = 0;
	pOpt->RIM = 1;
	pOpt->RIV = 0b10111;

	/* LEFT HP CHANNEL */
	pOpt->LRS_HP = 0;
	pOpt->LZC = 1;
	pOpt->LHV = 0b1111001;

	/* RIGHT HP CHANNEL */
	pOpt->RLS_HP = 0;
	pOpt->RZC = 1;
	pOpt->RHV = 0b1111001;

	/* ANALOG AUDIO PATH */
	pOpt->STA_STE = 0;
	pOpt->DAC_SEL = 0;
	pOpt->BYP = 1;
	pOpt->INSEL = 0;
	pOpt->MICM = 1;
	pOpt->MICB = 0;

	/* DIGITAL AUDIO PATH */
	pOpt->DACM = 1;
	pOpt->DEEMP = 0;
	pOpt->ADCHP = 0;

	/* POWER DOWN CONTROL */
	pOpt->PD_OFF = 0;
	pOpt->PD_CLK = 0;
	pOpt->PD_OSC = 0;
	pOpt->PD_OUT = 0;
	pOpt->PD_DAC = 0;
	pOpt->PD_ADC = 1;
	pOpt->PD_MIC = 1;
	pOpt->PD_LINE = 1;

	/* DIGITAL AUDIO INTERFACE FORMAT */
	pOpt->MS = 0;
	pOpt->LRSWAP = 0;
	pOpt->LRP = 0;
	pOpt->IWL = 0;
	pOpt->FOR = 1;

	/* SAMPLE RATE CONTROL */
	pOpt->CLKIN = 0;
	pOpt->CLKOUT = 0;
	pOpt->SR = 8;
	pOpt->BOSR = 0;
	pOpt->USB_Normal = 0;


	// digital interface activation
	//TLV320_write(0x09, 1);

	// reset device
	TLV320_write(0x0F, 0);

	// digital interface activation
	//TLV320_write(0x09, 1);
}

void TLV320_getOptions(TLV320_Options *pOpt) {
	*pOpt = sState.options;
}

void TLV320_activate() {
	TLV320_write(0x09, 1);
}

void TLV320_fullUpdate(TLV320_Options *pOpt) {
	sFullUpdate = true;
	TLV320_updateOptions(pOpt);
}

#define _MAX(x,y) ((x > y) ? (x) : (y))
#define _MIN(x,y) ((x < y) ? (x) : (y))
#define _LIMIT(x,l,h) _MIN(_MAX(x,l),h)

#define TLV320_GAIN_MIN_CB (-345)
#define TLV320_GAIN_MAX_CB (120)
#define TLV320_GAIN_MIN_VAL (0)
#define TLV320_GAIN_MAX_VAL (31)
#define TLV320_GAIN_QUANTUM_CB (15)
#define TLV320_GAIN_UNITY_VAL (23)

void TLV320_setLineInGain(double dB) {
	int cB = dB * 10; // deciBel -> centiBel

	// calculate register value
	uint8_t val = cB / TLV320_GAIN_QUANTUM_CB + TLV320_GAIN_UNITY_VAL;

	// limit clipping
	val = _LIMIT(val, TLV320_GAIN_MIN_VAL, TLV320_GAIN_MAX_VAL);

	// push new volume value
	sState.options.RIV = sState.options.LIV = val;

	// update options
	TLV320_updateOptions(&sState.options);
}
