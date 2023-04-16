/*
 * tlv320.h
 *
 *  Created on: 2021. okt. 24.
 *      Author: epagris
 */

#ifndef TLV320_TLV320_H_
#define TLV320_TLV320_H_

#include "stm32h7xx_hal.h"

#define TLV320_I2C_ADDR0 (26 << 1)
#define TLV320_I2C_ADDR1 (27 << 1)

typedef struct {
	/* LEFT LINE INPUT */
	uint8_t LIV; // Left Line Input Channel volume (0-31, 23 = 0dB)
	bool LRS; // Left/Right line simultaneous volume/mute update
	bool LIM; // Left input line mute

	/* RIGHT LINE INPUT */
	uint8_t RIV; // Right Line Input Channel volume (0-31, 23 = 0dB)
	bool RLS; // Right/Left line simultaneous volume/mute update
	bool RIM; // Right input line mute

	/* LEFT HEADPHONE OUTPUT */
	uint8_t LHV; // Left Line Input Channel volume (48-127, 121 = 0dB)
	bool LRS_HP; // Left/Right line simultaneous volume/mute update
	bool LZC; // Left-channel zero-cross detect

	/* RIGHT HEADPHONE OUTPUT */
	uint8_t RHV; // Right Line Input Channel volume (48-127, 121 = 0dB)
	bool RLS_HP; // Right/Left line simultaneous volume/mute update
	bool RZC; // Right-channel zero-cross detect

	/* ANALOG AUDIO PATH CONTROL */
	uint8_t STA_STE; // Added sidetone (See manual.)
	bool DAC_SEL; // DAC select
	bool BYP; // Bypass on/off (line in -> headphone)
	bool INSEL; // Input select for ADC (0: line, 1: microphone)
	bool MICM; // Microphone mute (0: normal, 1: muted)
	bool MICB; // Microphone boost (0: 0 dB, 1: 20 dB)

	/* DIGITAL AUDIO PATH CONTROL */
	bool DACM; // DAC soft mute
	uint8_t DEEMP; // De-emphasis control (See manual.)
	bool ADCHP; // ADC high-pass filter on/off

	/* POWER DOWN CONTROL (0: On, 1: Off) */
	bool PD_OFF; // Device power
	bool PD_CLK; // Clock
	bool PD_OSC; // Oscillator
	bool PD_OUT; // Outputs
	bool PD_DAC; // DAC
	bool PD_ADC; // ADC
	bool PD_MIC; // Microphone input
	bool PD_LINE; // Line input

	/* DIGITAL AUDIO INTERFACE FORMAT */
	bool MS; // Master/Slave mode (0: slave, 1: master)
	bool LRSWAP; // DAC left/right swap on/off
	bool LRP; // DAC left/right phase (See manual.)
	uint8_t IWL; // Input bit length (00: 16 bit, 01: 20 bit, 10: 24 bit, 11: 32 bit)
	uint8_t FOR; // Data format (See manual.)

	/* SAMPLE RATE CONTROL */
	bool CLKIN; // Clock input divider (0: MCLK, 1: MCLK/2)
	bool CLKOUT; // Clock input divider (0: MCLK, 1: MCLK/2)
	uint8_t SR; // Sampling rate control (See manual.)
	bool BOSR; // Base oversamping rate (See manual.)
	bool USB_Normal; // Clock mode select (0: Normal, 1: USB)

} TLV320_Options;

typedef struct {
	uint8_t i2c_addr; // az eszköz I2C-címe
	TLV320_Options options;
} TLV320_State;

void TLV320_init(I2C_HandleTypeDef *phI2C, uint8_t i2c_addr);
void TLV320_updateOptions(TLV320_Options *pOpt);
void TLV320_reset();
void TLV320_getOptions(TLV320_Options *pOpt);
void TLV320_setOptions(const TLV320_Options *pOpt);
void TLV320_activate();
void TLV320_fullUpdate(TLV320_Options *pOpt);
void TLV320_setLineInGain(double dB);

#endif /* TLV320_TLV320_H_ */
