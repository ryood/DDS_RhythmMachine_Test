// DDS_RhythmMachine_Test.cpp
//
// ���Y���}�V���p�ɐU���ϒ��݂̂��s��
// 
// Wave:  DDS�Ő���
// Decay: ���`��ԂŐ���
//
// 2015.10.08 �Œ菬���_���Z �i�r���j
//

#include "stdafx.h"

#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>

#include "WaveTable16bit.h"
#include "fixedpoint.h"

/*********************************************************
lookupTable       : 16bit : -32768..32767

wavePhaseRegister : 32bit
waveTunigWord     : 32bit

ampAmount         : 8bit
toneAmount        : 8bit
decayAmount       : 8bit
bpmAmount         : 8bit
**********************************************************/
#define SAMPLE_CLOCK		(48000u)	// 48kHz
#define MAX_DECAY_LEN		(48000u)	// 1�b

//#define TRACK_N				(3u)		// �g���b�N�̌�
#define TRACK_N				(1u)		// �g���b�N�̌�
#define LOOKUP_TABLE_SIZE	(1024u)		// Lookup Table �̗v�f��

#define SEQUENCE_LEN		(16u)

#define POW_2_32			(4294967296.0f) // 2��32��

// �ϐ��̏����l
#define INITIAL_BPM			(120u)

// BPM
uint8_t bpm;				// 1���������beat�� (beat=note*4)
uint32_t noteTicks;			// note������̃T���v�����O��

// �Đ�����
int period = SAMPLE_CLOCK * 2;

struct track {
	int16_t *lookupTable;
	fp32 waveFrequency;
	uint8_t decayAmount;
	uint8_t ampAmount;
	uint8_t toneAmount;
	
	uint32_t phaseRegister;
	uint32_t tuningWord;
	fp32 waveValue;
	
	int32_t decayPeriod;
	int32_t decayCount;
	uint8_t decayStop;		// 1: decay�̒�~
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
	tracks[0].lookupTable = waveTableSine;
	tracks[0].waveFrequency = int_to_fp32(60);
	tracks[0].decayAmount = 127;
	tracks[0].ampAmount = 200;
	tracks[0].toneAmount = 127;
	tracks[0].decayCount = 0;
	tracks[0].decayStop = 0;
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
	int tick = -1;		// �����0�ɃC���N�������g
	int noteCount = 0;

	_setmode(_fileno(stdout), _O_BINARY);
	
	initTracks();

	// DDS�p�̕ϐ��̏�����
	for (int i = 0; i < TRACK_N; i++) {
		double dblTuningWord = fp32_to_double(tracks[i].waveFrequency) * POW_2_32 / SAMPLE_CLOCK;
		tracks[i].tuningWord = dblTuningWord;
		tracks[i].phaseRegister = 0;
		//printf("track:%d\ttunigWord:%u\tphaseRegister:%u\n", i, tracks[i].tuningWord, tracks[i].phaseRegister);

		// Decay�̍ő�l��ݒ�
		tracks[i].decayPeriod = tracks[i].decayAmount * MAX_DECAY_LEN / 256;
		//printf("decayPeriod:\t%d\t%d\n", i, tracks[i].decayPeriod);
	}

	// BPM�̌v�Z
	//
	bpm = INITIAL_BPM;
	//printf("bpm:\t%d\n", bpm);

	noteTicks = SAMPLE_CLOCK / (bpm * 4 / 60);
	//printf("noteTicks:\t%d\n", noteTicks);

	for (int i = 0; i < period; i++)
	{
		tick++;
		
		if (tick >= noteTicks) {
			noteCount++;
			//printf("%d\t%d\n", tick, noteCount);

			// beat�̐擪��tick�����Z�b�g
			tick = 0;

			// Beat�̐擪��decayCount�����Z�b�g
			for (int i = 0; i < TRACK_N; i++) {
				// Decay�̍ő�l��ݒ�
				tracks[i].decayCount = 0;
				tracks[i].decayStop = 0;
			}
		}
		//printf("%d\t%d\n", noteCount, tick);
		
		// �g���b�N�̏���
		//
		for (int j = 0; j < TRACK_N; j++) { 
			printf("tick:%d\t noteCount:%d\t track:%d\t decayCount:%d\n", tick, noteCount, j, tracks[j].decayCount);
			
			// Decay�̏��� ***********************************************************
			//
			//***********************************************************************
			if (!tracks[j].decayStop) {
				tracks[j].decayCount++;
			}
			if (tracks[j].decayCount == tracks[j].decayPeriod) {
				tracks[j].decayStop = 1;
			}
			// decayValue = 1.0f - decayCount / decayPeriod
			fp32 decayRev = fp32_div(int_to_fp32(tracks[j].decayCount), int_to_fp32(tracks[j].decayPeriod));
			tracks[j].decayValue = fp32_sub(int_to_fp32(1), decayRev);
			//printf("decayValue\ttrack:%d\t%f\n", j, fp32_to_double(tracks[j].decayValue));
			//printf("%d\t%f\n", tick, fp32_to_double(tracks[j].decayValue));

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
#endif
		}
#if 0
		
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
		}

		//�@for precise float output 
		//printf("%f", synthWaveValue);
		//printf("\n");
		
		// �o�͒l�̕␳ ***********************************************************
		//
		// ************************************************************************
		// for 12bit output (0..4096)
		int16_t output_12bit = (fp32_to_double(synthWaveValue) + 1.0f) * 2048;
		//printf("%d\n", output_12bit);

		// for 16bit output (-32768 .. 32767)
		int16_t output_16bit = fp32_to_double(synthWaveValue) * 32768;
		//printf("%d\n", output_16bit); 
		//fwrite(&output_16bit_raw, sizeof(output_16bit), 1, stdout);
#endif
	}
	
	return 0;
}
