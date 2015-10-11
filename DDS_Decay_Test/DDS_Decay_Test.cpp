// DDS_Decay_Test.cpp : Defines the entry point for the console application.
//
// Decay波形生成テスト
//
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
#define MOD_LOOKUP_TABLE_SIZE (128u)

#define POW_2_32			  (4294967296ul) // 2の32乗

// カウンター
int tick = -1;				// 初回に0にインクリメント
int noteCount = 0;

// BPM
uint8_t bpm = 240;			// 1分あたりのbeat数 (beat=note*4)

int32_t ticksPerNote;		// noteあたりのサンプリング数

int period = 10000;

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

	// DDS変数の初期化
	//decayPeriod = (SAMPLE_CLOCK / (((double)bpm / 60) * 4))  * ((double)decayAmount / 256);
	decayPeriod = ((uint32_t)SAMPLE_CLOCK * 60 * decayAmount) / ((uint32_t)bpm * 4 * 256);
	
	// decayの周波数は1note分
	//decayTuningWord = (((double)bpm / 60) * 4) * (uint64_t)POW_2_32 / SAMPLE_CLOCK;
	//decayTuningWord = bpm * ((uint64_t)POW_2_32 / 60) * decayAmount / (SAMPLE_CLOCK * 256);

	// decayの周波数にdecayAmountで重み付け
	decayTuningWord = ((((double)bpm / 60) * 4) / ((double)decayAmount / 256)) * (double)POW_2_32 / SAMPLE_CLOCK;
  	
	decayPhaseRegister = 0;
	printf("tunigWord:%u\tphaseRegister:%u\tperiod:%u\n", decayTuningWord, decayPhaseRegister, decayPeriod);
	decayStop = 0;

	for (int i = 0; i < period; i++) {
		
		tick++;

		if (tick >= ticksPerNote) {
			noteCount++;
			//printf("%d\t%d\n", tick, noteCount);
			
			// noteの先頭でtickをリセット
			tick = 0;

			// noteの先頭でdecay波形生成の再開
			//decayPhaseRegister = 0;
			decayStop = 0;
		}
		printf("%d\t%d\t", noteCount, tick);
		
		// DDS
		// decayPeriodでdecay波形の生成を終了
		if (!decayStop) {
			decayPhaseRegister += decayTuningWord;
		}
		if (tick == decayPeriod) {
			decayStop = 1;
			decayPhaseRegister = 0;
		}

		// 32bitのphaseRegisterをテーブルの7bit(128個)に丸める
		int decayIndex = decayPhaseRegister >> 25;
		printf("decayPhaseregister:\t%d\tdecayIndex:\t%d\n", decayPhaseRegister, decayIndex);
	}

	return 0;
}

