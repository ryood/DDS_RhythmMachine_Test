// DDS_Decay_Test.cpp : Defines the entry point for the console application.
//
// Decay�g�`�����e�X�g
//
// 2015.10.11 Decay�̒����ɍ��킹��Decay�̍Đ����g���ɏd�ݕt��
// 2015.10.11 Created decay��index�̑�����period�̏I���Œ��f����
//

#include "stdafx.h"

#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
#include "fixedpoint.h"
#include "ModTableFp32.h"

#define SAMPLE_CLOCK          (48000u)
//#define MOD_LOOKUP_TABLE_SIZE (128u)

#define POW_2_32			  (4294967296ull) // 2��32�� (64bit����)

// �J�E���^�[
int tick = -1;				// �����0�ɃC���N�������g
int noteCount = 0;

// BPM
uint8_t bpm = 120;			// 1���������beat�� (beat=note*4)

int32_t ticksPerNote;		// note������̃T���v�����O��

int period = 24000;

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

	ticksPerNote = SAMPLE_CLOCK * 60ul / (bpm * 4);
	// ���������Z�̂��ߊۂ߂Ă���̂Œ���
	printf("ticksPerNote:\t%d\n", ticksPerNote);

	// DDS�ϐ��̏�����------------------------------------------------------------------------
	//
	// ���������_���Z
	//decayPeriod = (SAMPLE_CLOCK / (((double)bpm / 60) * 4)) * ((double)decayAmount / 256);
	// �������Z(64bit)
	decayPeriod = ((uint64_t)SAMPLE_CLOCK * 60 * decayAmount) / ((uint64_t)bpm * 4 * 256);
	
	// decay�g�`�̎�����1note��
	// ���������_���Z
	//decayTuningWord = (((double)bpm / 60) * 4) * (uint64_t)POW_2_32 / SAMPLE_CLOCK;
	// �������Z(64bit)
	//decayTuningWord = bpm * ((uint64_t)POW_2_32 / 60) * 4 / SAMPLE_CLOCK;

	// decay�g�`�̎�����decayAmount�ŏd�ݕt��
	// ���������_���Z
	//decayTuningWord = ((((double)bpm / 60) * 4) / ((double)decayAmount / 256)) * (double)POW_2_32 / SAMPLE_CLOCK;
	// �������Z(64bit)
	decayTuningWord = (bpm * ((uint64_t)POW_2_32 / 60) * 4 * 256 / decayAmount) / SAMPLE_CLOCK;
 
	printf("decayAmount:\t%u\n", decayAmount);
	printf("decayPeriod:\t%u\n", decayPeriod);
	printf("decayTunigWord:\t%u\n", decayTuningWord);
	
	decayPhaseRegister = 0;
	decayStop = 0;

	for (int i = 0; i < period; i++) {
		
		tick++;

		if (tick >= ticksPerNote) {
			noteCount++;
			//printf("%d\t%d\n", tick, noteCount);
			
			// note�̐擪��tick�����Z�b�g
			tick = 0;

			// note�̐擪��decay�g�`�����̍ĊJ
			decayPhaseRegister = 0;
			decayStop = 0;
		}
		printf("%d\t%d\t", noteCount, tick);
		
		// DDS
		// decayPeriod��decay�g�`�̐������I��
		if (!decayStop) {
			decayPhaseRegister += decayTuningWord;
		}
		if (tick == decayPeriod - 1) {
			decayStop = 1;
		}

		// 32bit��phaseRegister���e�[�u����7bit(128��)�Ɋۂ߂�
		int decayIndex = decayPhaseRegister >> 25;
		printf("%d\t%d\t", decayPhaseRegister, decayIndex);

		decayValue = *(decayLookupTable + decayIndex);
		printf("%f\n", fp32_to_double(decayValue));
	}

	return 0;
}

