// DDS_RhythmMachine_Test.cpp
//
// リズムマシン用に振幅変調のみを行う
// 
// Wave:  DDSで生成
// Decay: DDSで生成
//
// 2015.10.11 DecayをDDS処理に変更
// 2015.10.11 Q16に変更
// 2015.10.08 固定小数点演算
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

//#define TRACK_N				(3u)		// トラックの個数
#define TRACK_N				  (1u)		// トラックの個数
#define LOOKUP_TABLE_SIZE	  (1024u)		// Lookup Table の要素数
#define MOD_LOOKUP_TABLE_SIZE (256u)
#define SEQUENCE_LEN		  (16u)

#define POW_2_32			  (4294967296.0f) // 2の32乗

// 変数の初期値
#define INITIAL_BPM			  (120u)

// カウンター
int tick = -1;				// 初回に0にインクリメント
int noteCount = 0;

// BPM
uint8_t bpm;				// 1分あたりのbeat数 (beat=note*4)
uint32_t noteTicks;			// noteあたりのサンプリング数

// 再生時間
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
// トラックの初期化
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

	// BPMの計算
	//
	bpm = INITIAL_BPM;
	printf("bpm:\t%d\n", bpm);

	noteTicks = SAMPLE_CLOCK * 60 / (bpm * 4);
	// ↑整数演算のため丸めているので注意
	printf("noteTicks:\t%d\n", noteTicks);

	// DDS用の変数の初期化
	for (int i = 0; i < TRACK_N; i++) {
		// 波形
		tracks[i].waveTuningWord = tracks[i].waveFrequency * POW_2_32 / SAMPLE_CLOCK;
		tracks[i].wavePhaseRegister = 0;
		printf("wave:%d\ttunigWord:%u\tphaseRegister:%u\n", i, tracks[i].waveTuningWord, tracks[i].wavePhaseRegister);

		// Decay
		tracks[i].decayPeriod = SAMPLE_CLOCK * (60 / 4) * tracks[i].decayAmount / (bpm * 256);
		tracks[i].decayTuningWord = bpm * (POW_2_32 / 60) * tracks[i].decayAmount / (SAMPLE_CLOCK * 256);
		// ↑整数演算のため誤差あり
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

			// noteの先頭でtickをリセット
			tick = 0;

			// noteの先頭でdecayPhaserRegisterをリセット
			for (int j = 0; j < TRACK_N; j++) {
				tracks[j].decayPhaseRegister = 0;
				tracks[j].decayStop = 0;
			}
		}
		printf("%d\t%d\t", noteCount, tick);
	
		// トラックの処理
		//
		for (int j = 0; j < TRACK_N; j++) { 
			
			// Decayの処理 ***********************************************************
			//
			//***********************************************************************
			//if (!tracks[j].decayStop) {
				tracks[j].decayPhaseRegister += tracks[j].decayTuningWord;
			//}
			if (tick == tracks[j].decayPeriod) {
				tracks[j].decayStop = 1;
			}
			
			// 32bitのphaseRegisterをテーブルの7bit(128個)に丸める
			int decayIndex = tracks[j].decayPhaseRegister >> 25;
			printf("decayPhaseregister:\t%d\tdecayIndex:\t%d\n", tracks[j].decayPhaseRegister, decayIndex);
			
			tracks[j].decayValue = *(tracks[j].decayLookupTable + decayIndex);
			//printf("decayValue\ttrack:%d\t%f\n", j, fp32_to_double(tracks[j].decayValue));
			//printf("%d\t%f\n", tick, fp32_to_double(tracks[j].decayValue));
			//printf("tick:%d\t noteCount:%d\t track:%d\t decayPhaseRegister:%d\n", tick, noteCount, j, tracks[j].decayPhaseRegister);
#if 0	
			// サンプル毎の振幅変調の合算 *************************************************
			//
			//************************************************************************ 
			fp32 amValue = tracks[i].decayValue;
			printf("tick:\t%d\tTrack:\t%d\tamValue:\t%f\n", i, fp32_to_double(amValue));
			
			// Wave系の処理 ***********************************************************
			//
			//************************************************************************
			tracks[j].phaseRegister += tracks[j].tuningWord;
			printf("phaseRegister:\t%d\n", tracks[j].phaseRegister);
		
			// lookupTableの要素数に丸める
			// 32bit -> 10bit
			uint16_t index = tracks[j].phaseRegister >> 22;
			printf("index:\t%d\n", index);

			tracks[j].waveValue = int_to_fp32(*(tracks[j].lookupTable + index));
			printf("waveValue:\t%f\n", tracks[i].waveValue);

			// waveValueを正規化 (-1.0 .. 1.0)
//			tracks[j].waveValue = fp32_div(tracks[j].waveValue, (INT16_MAX+1));
			//printf("waveValue:\t%f\n", tracks[i].waveValue);
			//printf("%f\t", tracks[i].waveValue);
			
			// 振幅変調 --------------------------------------------------------------
			//
//			tracks[j].waveValue = fp32_mul(tracks[j].waveValue, amValue);
			//printf("%f\t", tracks[i].waveValue);

		}
		
		// トラックの合成
		//
		//printf("%d", noteCount % SEQUENCE_LEN);
		
		fp32 synthWaveValue = int_to_fp32(0);
		for (int i = 0; i < TRACK_N; i++) {
			// 各トラックの出力値： waveValue * sequence[note](Velocity) * ampAmount
			fp32 v = fp32_mul(tracks[i].waveValue, ((fp32)tracks[i].sequence[noteCount % SEQUENCE_LEN]));
			v = fp32_mul(v, ((fp32)tracks[i].ampAmount / UINT8_MAX));
			//printf("%f\t", v);
			synthWaveValue = fp32_add(synthWaveValue, v); 
#endif
		}

		//　for precise float output 
		//printf("%f", synthWaveValue);
		//printf("\n");
		
		// 出力値の補正 ***********************************************************
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
