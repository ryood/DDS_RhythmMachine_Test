// DDS_Decay_Test.cpp : Defines the entry point for the console application.
//
// Decay�g�`�����e�X�g
//
// 2015.10.11 Created
//

#include "stdafx.h"

#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
#include "fixedpoint.h"
#include "ModTableFp32.h"

#define SAMPLE_CLOCK          (48000u)
#define MOD_LOOKUP_TABLE_SIZE (128u)

#define POW_2_32			  (4294967296ul) // 2��32��

// �J�E���^�[
int tick = -1;				// �����0�ɃC���N�������g
int noteCount = 0;

// BPM
uint8_t bpm = 120;			// 1���������beat�� (beat=note*4)

uint32_t noteTicks;			// note������̃T���v�����O��

int period = 3000;

// Parameter
const fp32 *decayLookupTable;
uint8_t decayAmount = 127;

uint32_t decayPhaseRegister;
uint32_t decayTuningWord;
uint32_t decayPeriod;
uint8_t decayStop;
fp32 decayValue;

int _tmain(int argc, _TCHAR* argv[])
{
	_setmode(_fileno(stdout), _O_BINARY);

	decayLookupTable = modTableDown;

	// BPM�̌v�Z
	//
	printf("bpm:\t%d\n", bpm);

	noteTicks = SAMPLE_CLOCK * 60ul / (bpm * 4);
	// ���������Z�̂��ߊۂ߂Ă���̂Œ���
	printf("noteTicks:\t%d\n", noteTicks);

	// DDS�ϐ��̏�����
	//decayPeriod = (SAMPLE_CLOCK / (((double)bpm / 60) * 4))  * ((double)decayAmount / 256);
	decayPeriod = ((uint32_t)SAMPLE_CLOCK * 60 * decayAmount) / ((uint32_t)bpm * 4 * 256);
	decayTuningWord = bpm * ((uint64_t)POW_2_32 / 60) * decayAmount / (SAMPLE_CLOCK * 256);
	decayPhaseRegister = 0;
	printf("tunigWord:%u\tphaseRegister:%u\tperiod:%u\n", decayTuningWord, decayPhaseRegister, decayPeriod);
	decayStop = 0;

	return 0;
}

