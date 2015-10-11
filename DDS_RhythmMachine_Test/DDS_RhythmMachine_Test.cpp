// DDS_RhythmMachine_Test.cpp
//
// ���Y���}�V���p�ɐU���ϒ��݂̂��s��
// 
// Wave:  DDS�Ő���
// Decay: DDS�Ő���
//
// 2015.10.11 Decay��DDS�����ɕύX
// 2015.10.11 Q16�ɕύX
// 2015.10.08 �Œ菬���_���Z
//

#include "stdafx.h"

#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>

#include "fixedpoint.h"
#include "WaveTableFp32.h"
#include "ModTableFp32.h"


/*********************************************************
waveLookupTable   : fp32 Q16 : -1.0 .. +1.0
decayLookupTable  : fp32 Q16 : -1.0 .. +1.0

wavePhaseRegister : 32bit
waveTunigWord     : 32bit
decayPhaseRegister: 32bit
decayTuningWord   : 32bit

waveFrequency     : double
ampAmount         : 8bit
toneAmount        : 8bit
decayAmount       : 8bit
bpmAmount         : 8bit
**********************************************************/
#define SAMPLE_CLOCK		  (48000u)	// 48kHz

//#define TRACK_N				(3u)		// �g���b�N�̌�
#define TRACK_N				  (1u)		// �g���b�N�̌�
#define LOOKUP_TABLE_SIZE	  (1024u)		// Lookup Table �̗v�f��
#define MOD_LOOKUP_TABLE_SIZE (256u)
#define SEQUENCE_LEN		  (16u)

#define POW_2_32			  (4294967296.0f) // 2��32��

// �ϐ��̏����l
#define INITIAL_BPM			  (120u)

// �J�E���^�[
int tick = -1;				// �����0�ɃC���N�������g
int noteCount = 0;

// BPM
uint8_t bpm;				// 1���������beat�� (beat=note*4)
uint32_t noteTicks;			// note������̃T���v�����O��

// �Đ�����
int period = 30000;

struct track {
	const fp32 *waveLookupTable;
	const fp32 *decayLookupTable;
	double waveFrequency;
	uint8_t decayAmount;
	uint8_t ampAmount;
	uint8_t toneAmount;
	
	uint32_t wavePhaseRegister;
	uint32_t waveTuningWord;
	fp32 waveValue;
	
	uint32_t decayPhaseRegister;
	uint32_t decayTuningWord;
	uint32_t decayPeriod;
	uint8_t decayStop;
	fp32 decayValue;
	
	uint8_t sequence[SEQUENCE_LEN];	// Velocity
} tracks[TRACK_N];

//*******************************************************************
// �g���b�N�̏�����
//
void initTracks()
{
	const uint8_t kickSequence[]  = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0 };

#if 0
	const uint8_t snareSequence[] = { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 };
	const uint8_t hihatSequnce[]  = { 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1 };
#endif
	// Kick
	tracks[0].waveLookupTable = waveTableSine;
	tracks[0].decayLookupTable = modTableDown;
	tracks[0].waveFrequency = 60.0f;
	tracks[0].decayAmount = 255;
	tracks[0].ampAmount = 200;
	tracks[0].toneAmount = 127;
	memcpy(tracks[0].sequence, kickSequence, SEQUENCE_LEN);

#if 0
	// Snare
	tracks[1].lookupTable = waveTableSine;
	tracks[1].waveFrequency = int_to_fp32(200);
	tracks[1].decayAmount = 32;
	tracks[1].ampAmount = 127;
	tracks[1].toneAmount = 127;
	tracks[1].decayCount = 0;
	tracks[1].decayStop = 0;
	memcpy(tracks[1].sequence, snareSequence, SEQUENCE_LEN);

	// HiHat
	tracks[2].lookupTable = waveTableWhiteNoise;
	tracks[2].waveFrequency = int_to_fp32(10);
	tracks[2].decayAmount = 8;
	tracks[2].ampAmount = 64;
	tracks[2].toneAmount = 127;
	tracks[2].decayCount = 0;
	tracks[2].decayStop = 0;
	memcpy(tracks[2].sequence, hihatSequnce, SEQUENCE_LEN);
#endif
}

int _tmain(int argc, _TCHAR* argv[])
{
	_setmode(_fileno(stdout), _O_BINARY);
	
	initTracks();

	// BPM�̌v�Z
	//
	bpm = INITIAL_BPM;
	printf("bpm:\t%d\n", bpm);

	noteTicks = SAMPLE_CLOCK * 60 / (bpm * 4);
	// ���������Z�̂��ߊۂ߂Ă���̂Œ���
	printf("noteTicks:\t%d\n", noteTicks);

	// DDS�p�̕ϐ��̏�����
	for (int i = 0; i < TRACK_N; i++) {
		// �g�`
		tracks[i].waveTuningWord = tracks[i].waveFrequency * POW_2_32 / SAMPLE_CLOCK;
		tracks[i].wavePhaseRegister = 0;
		printf("wave:%d\ttunigWord:%u\tphaseRegister:%u\n", i, tracks[i].waveTuningWord, tracks[i].wavePhaseRegister);

		// Decay
		tracks[i].decayPeriod = SAMPLE_CLOCK * (60 / 4) * tracks[i].decayAmount / (bpm * 256);
		tracks[i].decayTuningWord = bpm * (POW_2_32 / 60) * tracks[i].decayAmount / (SAMPLE_CLOCK * 256);
		// ���������Z�̂��ߌ덷����
		tracks[i].decayPhaseRegister = 0;
		printf("decay:%d\ttunigWord:%u\tphaseRegister:%u\tperiod:%u\n", 
			i, tracks[i].decayTuningWord, tracks[i].decayPhaseRegister, tracks[i].decayPeriod);
		tracks[i].decayStop = 0;
	}

	for (int i = 0; i < period; i++)
	{
		tick++;
		
		if (tick >= noteTicks) {
			noteCount++;
			//printf("%d\t%d\n", tick, noteCount);

			// note�̐擪��tick�����Z�b�g
			tick = 0;

			// note�̐擪��decayPhaserRegister�����Z�b�g
			for (int j = 0; j < TRACK_N; j++) {
				tracks[j].decayPhaseRegister = 0;
				tracks[j].decayStop = 0;
			}
		}
		printf("%d\t%d\t", noteCount, tick);
	
		// �g���b�N�̏���
		//
		for (int j = 0; j < TRACK_N; j++) { 
			
			// Decay�̏��� ***********************************************************
			//
			//***********************************************************************
			//if (!tracks[j].decayStop) {
				tracks[j].decayPhaseRegister += tracks[j].decayTuningWord;
			//}
			if (tick == tracks[j].decayPeriod) {
				tracks[j].decayStop = 1;
			}
			
			// 32bit��phaseRegister���e�[�u����7bit(128��)�Ɋۂ߂�
			int decayIndex = tracks[j].decayPhaseRegister >> 25;
			printf("decayPhaseregister:\t%d\tdecayIndex:\t%d\n", tracks[j].decayPhaseRegister, decayIndex);
			
			tracks[j].decayValue = *(tracks[j].decayLookupTable + decayIndex);
			//printf("decayValue\ttrack:%d\t%f\n", j, fp32_to_double(tracks[j].decayValue));
			//printf("%d\t%f\n", tick, fp32_to_double(tracks[j].decayValue));
			//printf("tick:%d\t noteCount:%d\t track:%d\t decayPhaseRegister:%d\n", tick, noteCount, j, tracks[j].decayPhaseRegister);
#if 0	
			// �T���v�����̐U���ϒ��̍��Z *************************************************
			//
			//************************************************************************ 
			fp32 amValue = tracks[i].decayValue;
			printf("tick:\t%d\tTrack:\t%d\tamValue:\t%f\n", i, fp32_to_double(amValue));
			
			// Wave�n�̏��� ***********************************************************
			//
			//************************************************************************
			tracks[j].phaseRegister += tracks[j].tuningWord;
			printf("phaseRegister:\t%d\n", tracks[j].phaseRegister);
		
			// lookupTable�̗v�f���Ɋۂ߂�
			// 32bit -> 10bit
			uint16_t index = tracks[j].phaseRegister >> 22;
			printf("index:\t%d\n", index);

			tracks[j].waveValue = int_to_fp32(*(tracks[j].lookupTable + index));
			printf("waveValue:\t%f\n", tracks[i].waveValue);

			// waveValue�𐳋K�� (-1.0 .. 1.0)
//			tracks[j].waveValue = fp32_div(tracks[j].waveValue, (INT16_MAX+1));
			//printf("waveValue:\t%f\n", tracks[i].waveValue);
			//printf("%f\t", tracks[i].waveValue);
			
			// �U���ϒ� --------------------------------------------------------------
			//
//			tracks[j].waveValue = fp32_mul(tracks[j].waveValue, amValue);
			//printf("%f\t", tracks[i].waveValue);

		}
		
		// �g���b�N�̍���
		//
		//printf("%d", noteCount % SEQUENCE_LEN);
		
		fp32 synthWaveValue = int_to_fp32(0);
		for (int i = 0; i < TRACK_N; i++) {
			// �e�g���b�N�̏o�͒l�F waveValue * sequence[note](Velocity) * ampAmount
			fp32 v = fp32_mul(tracks[i].waveValue, ((fp32)tracks[i].sequence[noteCount % SEQUENCE_LEN]));
			v = fp32_mul(v, ((fp32)tracks[i].ampAmount / UINT8_MAX));
			//printf("%f\t", v);
			synthWaveValue = fp32_add(synthWaveValue, v); 
#endif
		}

		//�@for precise float output 
		//printf("%f", synthWaveValue);
		//printf("\n");
		
		// �o�͒l�̕␳ ***********************************************************
		//
		// ************************************************************************
		// for 12bit output (0..4096)
//		int16_t output_12bit = (fp32_to_double(synthWaveValue) + 1.0f) * 2048;
		//printf("%d\n", output_12bit);

		// for 16bit output (-32768 .. 32767)
//		int16_t output_16bit = fp32_to_double(synthWaveValue) * 32768;
		//printf("%d\n", output_16bit); 
		//fwrite(&output_16bit_raw, sizeof(output_16bit), 1, stdout);
	}	
	return 0;
}
