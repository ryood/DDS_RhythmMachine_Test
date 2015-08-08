// DDS_RhythmMachine_Test.cpp
//
// リズムマシン用に振幅変調のみを行う
// 
// Wave:  DDSで生成
// Decay: 線形補間で生成
//

#include "stdafx.h"

#include <stdint.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>

#include "WaveTable16bit.h"

/*********************************************************
lookupTable       : 16bit : 0..65535

wavePhaseRegister : 32bit
waveTunigWord     : 32bit

ampAmount         : 8bit
toneAmount        : 8bit
decayAmount       : 8bit
bpmAmount         : 8bit
**********************************************************/
#define SAMPLE_CLOCK		(48000u)	// 48kHz
#define MAX_DECAY_LEN		(48000u)	// 1秒 : 60BPM

#define TRACK_N				(3u)		// トラックの個数
#define LOOKUP_TABLE_SIZE	(1024u)		// Lookup Table の要素数

#define SEQUENCE_LEN		(16u)

// BPM
uint8_t bpm = 120;
// 再生時間
int period = SAMPLE_CLOCK;

int tick = 0;

struct track {
	int16_t *lookupTable;
	double waveFrequency;
	uint8_t decayAmount;
	uint8_t ampAmount;
	uint8_t toneAmount;
	
	uint32_t phaseRegister;
	uint32_t tuningWord;
	double waveValue;
	
	int decayPeriod;
	int decayCount;
	int decayStop;		// 1: decayの停止
	double decayValue;
	
	uint8_t sequence[SEQUENCE_LEN];	// Velocity
} tracks[TRACK_N];

enum trackName { kick, snare, hihat };

//*******************************************************************
// トラックの初期化
//
void initTracks()
{
	const uint8_t kickSequence[]  = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0 };
	const uint8_t snareSequence[] = { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 };
	const uint8_t hihatSequnce[]  = { 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1 };

	// Kick
	tracks[kick].lookupTable = waveTableSine;
	tracks[kick].waveFrequency = 60.0f;
	tracks[kick].decayAmount = 127;
	tracks[kick].ampAmount = 127;
	tracks[kick].toneAmount = 127;
	tracks[kick].decayCount = 0;
	tracks[kick].decayStop = 0;
	memcpy(tracks[kick].sequence, kickSequence, SEQUENCE_LEN);

	// Snare
	tracks[snare].lookupTable = waveTableSnare;
	tracks[snare].waveFrequency = 200.0f;
	tracks[snare].decayAmount = 127;
	tracks[snare].ampAmount = 127;
	tracks[snare].toneAmount = 127;
	tracks[snare].decayCount = 0;
	tracks[snare].decayStop = 0;
	memcpy(tracks[snare].sequence, snareSequence, SEQUENCE_LEN);

	// HiHat
	tracks[hihat].lookupTable = waveTableWhiteNoise;
	tracks[hihat].waveFrequency = 1000.0f;
	tracks[hihat].decayAmount = 127;
	tracks[hihat].ampAmount = 127;
	tracks[hihat].toneAmount = 127;
	tracks[hihat].decayCount = 0;
	tracks[hihat].decayStop = 0;
	memcpy(tracks[hihat].sequence, hihatSequnce, SEQUENCE_LEN);
}

int _tmain(int argc, _TCHAR* argv[])
{
	double bps = (double)bpm / 60;
	int beatCount = 0;

	_setmode(_fileno(stdout), _O_BINARY);
	
	initTracks();

	// DDS用の変数の初期化
	for (int i = kick; i <= hihat; i++) {
		tracks[i].tuningWord = tracks[i].waveFrequency * pow(2.0f, 32) / SAMPLE_CLOCK;
		tracks[i].phaseRegister = 0;
		
		// Decayの最大値を設定
		tracks[i].decayPeriod = tracks[i].decayAmount * MAX_DECAY_LEN / 256;
		//printf("decayPeriod:\t%d\t%d\n", i, tracks[i].decayPeriod);
	}

	for (int i = 0; i < period; i++)
	{
		if (tick % (int)(SAMPLE_CLOCK /  bps) == 0) {
			// ↑整数演算のために丸めているので注意

			beatCount++;
			//printf("%d\t%d\n", tick, beatCount);

			// Beatの先頭でdecayCountをリセット
			for (int i = kick; i <= hihat; i++) {
				// Decayの最大値を設定
				tracks[i].decayCount = 0;
				tracks[i].decayStop = 0;
			}
		}
		tick++;
		//printf("%d\n", tick);
		
		// トラックの処理
		//
		for (int i = kick; i <= hihat; i++) { 
			//printf("%d\t%d\n", beatCount, tracks[i].decayCount);
			
			// Decayの処理 ***********************************************************
			//
			//***********************************************************************
			if (!tracks[i].decayStop) {
				tracks[i].decayCount++;
			}
			if (tracks[i].decayCount == tracks[i].decayPeriod) {
				tracks[i].decayStop = 1;
			}
			tracks[i].decayValue = 1.0f - (double)tracks[i].decayCount / tracks[i].decayPeriod;
			//printf("%f\t", tracks[i].decayValue);
			
			double amValue = tracks[i].decayValue;
			
			// Wave系の処理 ***********************************************************
			//
			//************************************************************************
			tracks[i].phaseRegister += tracks[i].tuningWord;
			//printf("wavePhaseRegister:\t%d\n", tracks[i].wavePhaseRegister);
		
			// lookupTableの要素数に丸める
			// 32bit -> 10bit
			uint16_t index = tracks[i].phaseRegister >> 22;
			//printf("index:\t%d\n", index);

			tracks[i].waveValue = *(tracks[i].lookupTable + index);
			//printf("waveValue:\t%f\n", tracks[i].waveValue);

			// 浮動小数点に変換  (-1.0 .. 1.0)
			tracks[i].waveValue = 2.0f * tracks[i].waveValue / INT16_MAX;
			//printf("waveValue:\t%f\n", tracks[i].waveValue);
			//printf("%f\t", tracks[i].waveValue);
			
			// 振幅変調 --------------------------------------------------------------
			//
			tracks[i].waveValue *= amValue;
			//printf("%f\n", tracks[i].waveValue);
		}
		
		// トラックの合成
		//
		double waveValue = 0.0f;
		for (int i = kick; i <= hihat; i++) {
			double v = tracks[i].waveValue
					* tracks[i].sequence[beatCount % SEQUENCE_LEN]
					* tracks[i].ampAmount / UINT8_MAX;
					
			printf("%f\t", v);
			waveValue += v; 
		}
		printf("\n");
		
		// 出力値の補正 ***********************************************************
		//
		// ************************************************************************
		// for 12bit output (0..4096)
		int16_t output_12bit = (waveValue + 1.0f) * 2048;
		//printf("%d\n", output_12bit);

		// for 16bit output (-32768 .. 32767)
		int16_t output_16bit = waveValue * 32768;
		//printf("%d\n", output_16bit); 
		//fwrite(&output_16bit_raw, sizeof(output_16bit), 1, stdout);
	}
	return 0;
}
