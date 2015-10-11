// DDS_Decay_Test.cpp : Defines the entry point for the console application.
//
// Decay波形生成テスト
//
// 2015.10.11 Decayの長さに合わせてDecayの再生周波数に重み付け
// 2015.10.11 Created decayのindexの増加はperiodの終了で中断する
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

#define POW_2_32			  (4294967296ull) // 2の32乗 (64bit整数)

// カウンター
int tick = -1;				// 初回に0にインクリメント
int noteCount = 0;

// BPM
uint8_t bpm = 120;			// 1分あたりのbeat数 (beat=note*4)

int32_t ticksPerNote;		// noteあたりのサンプリング数

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

	// BPMの計算
	//
	printf("bpm:\t%d\n", bpm);

	ticksPerNote = SAMPLE_CLOCK * 60ul / (bpm * 4);
	// ↑整数演算のため丸めているので注意
	printf("ticksPerNote:\t%d\n", ticksPerNote);

	// DDS変数の初期化------------------------------------------------------------------------
	//
	// 浮動小数点演算
	//decayPeriod = (SAMPLE_CLOCK / (((double)bpm / 60) * 4)) * ((double)decayAmount / 256);
	// 整数演算(64bit)
	decayPeriod = ((uint64_t)SAMPLE_CLOCK * 60 * decayAmount) / ((uint64_t)bpm * 4 * 256);
	
	// decay波形の周期は1note分
	// 浮動小数点演算
	//decayTuningWord = (((double)bpm / 60) * 4) * (uint64_t)POW_2_32 / SAMPLE_CLOCK;
	// 整数演算(64bit)
	//decayTuningWord = bpm * ((uint64_t)POW_2_32 / 60) * 4 / SAMPLE_CLOCK;

	// decay波形の周期にdecayAmountで重み付け
	// 浮動小数点演算
	//decayTuningWord = ((((double)bpm / 60) * 4) / ((double)decayAmount / 256)) * (double)POW_2_32 / SAMPLE_CLOCK;
	// 整数演算(64bit)
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
			
			// noteの先頭でtickをリセット
			tick = 0;

			// noteの先頭でdecay波形生成の再開
			decayPhaseRegister = 0;
			decayStop = 0;
		}
		printf("%d\t%d\t", noteCount, tick);
		
		// DDS
		// decayPeriodでdecay波形の生成を終了
		if (!decayStop) {
			decayPhaseRegister += decayTuningWord;
		}
		if (tick == decayPeriod - 1) {
			decayStop = 1;
		}

		// 32bitのphaseRegisterをテーブルの7bit(128個)に丸める
		int decayIndex = decayPhaseRegister >> 25;
		printf("%d\t%d\t", decayPhaseRegister, decayIndex);

		decayValue = *(decayLookupTable + decayIndex);
		printf("%f\n", fp32_to_double(decayValue));
	}

	return 0;
}

