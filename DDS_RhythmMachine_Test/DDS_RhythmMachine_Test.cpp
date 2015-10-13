// DDS_RhythmMachine_Test.cpp
//
// リズムマシン用に振幅変調のみを行う
// 
// Wave:  DDSで生成
// Decay: DDSで生成
//
// 2015.10.13 Trackの合成
// 2015.10.13 DDS_Decay_Testの結果を反映
// 2015.10.11 DecayをDDS処理に変更
// 2015.10.11 Q16に変更
// 2015.10.08 固定小数点演算
//

#include "stdafx.h"

#include <stdio.h>
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
#define SAMPLE_CLOCK			(48000u)	// 48kHz

#define TRACK_N					(3u)		// トラックの個数
#define WAVE_LOOKUP_TABLE_SIZE	(1024u)		// Lookup Table の要素数
#define MOD_LOOKUP_TABLE_SIZE	(128u)
#define SEQUENCE_LEN		 	(16u)

#define POW_2_32				(4294967296ull) // 2の32乗

// 変数の初期値
#define INITIAL_BPM				(120u)

// カウンター
int tick = -1;				// 初回に0にインクリメント
int noteCount = 0;

// BPM
uint8_t bpm;				// 1分あたりのbeat数 (beat=note*4)
uint32_t ticksPerNote;			// noteあたりのサンプリング数

// 再生時間
int period = 96000;

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
	const uint8_t snareSequence[] = { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 };
	const uint8_t hihatSequnce[]  = { 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1 };

	// Kick
	tracks[0].waveLookupTable = waveTableSine;
	tracks[0].decayLookupTable = modTableDown;
	tracks[0].waveFrequency = 60.0f;
	tracks[0].decayAmount = 200;
	tracks[0].ampAmount = 255;
	tracks[0].toneAmount = 127;
	memcpy(tracks[0].sequence, kickSequence, SEQUENCE_LEN);

	// Snare
	tracks[1].waveLookupTable = waveTableSine;
	tracks[1].decayLookupTable = modTableDown;
	tracks[1].waveFrequency = 120.0f;
	tracks[1].decayAmount = 128;
	tracks[1].ampAmount = 255;
	tracks[1].toneAmount = 127;
	memcpy(tracks[1].sequence, snareSequence, SEQUENCE_LEN);

	// HiHat
	tracks[2].waveLookupTable = 0;				// unused
	tracks[2].decayLookupTable = modTableDown;
	tracks[2].waveFrequency = 0.0f;				// unused
	tracks[2].decayAmount = 24;
	tracks[2].ampAmount = 24;
	tracks[2].toneAmount = 127;
	memcpy(tracks[2].sequence, hihatSequnce, SEQUENCE_LEN);
}

//*******************************************************************
// 波形の生成
//
fp32 generateDDSWave(uint32_t *phaseRegister, uint32_t tuningWord, const fp32 *lookupTable)
{
	*phaseRegister += tuningWord;

	// lookupTableの要素数に丸める
	// 32bit -> 10bit
	uint16_t index = (*phaseRegister) >> 22;
	fp32 waveValue = *(lookupTable + index);
	/*
	printf("%d\t", *phaseRegister);
	printf("%d\t", index);
	printf("%f\t", fp32_to_double(waveValue));
	*/
	return waveValue;
}

fp32 generateNoise()
{
	uint32_t r, v;
	fp32 fv;
	
	r = (uint32_t)rand() << 1;
	v = (r & 0x8000) ? (0xffff0000 | (r << 1)) : (r << 1);
	fv = (fp32)v;
	//printf("%u\t%d\t%f\t", r, v, fp32_to_double(fv));

	return fv;
}

//*******************************************************************
// メインルーチン
//
int _tmain(int argc, _TCHAR* argv[])
{
	_setmode(_fileno(stdout), _O_BINARY);
	
	initTracks();

	// BPMの計算
	//
	bpm = INITIAL_BPM;
	//printf("bpm:\t%d\n", bpm);

	ticksPerNote = SAMPLE_CLOCK * 60 / (bpm * 4);
	// ↑整数演算のため丸めているので注意
	//printf("ticksPerNote:\t%d\n", ticksPerNote);

	// DDS用の変数の初期化
	for (int i = 0; i < TRACK_N; i++) {
		// 波形
		tracks[i].waveTuningWord = tracks[i].waveFrequency * POW_2_32 / SAMPLE_CLOCK;
		tracks[i].wavePhaseRegister = 0;
		/*
		printf("track:\t%d\n", i);
		printf("waveFrequency:\t%f\n", tracks[i].waveFrequency);
		printf("waveTunigWord:\t%u\n", tracks[i].waveTuningWord);
		*/
		// Decay
		//decayPeriod = (SAMPLE_CLOCK / (((double)bpm / 60) * 4)) * ((double)decayAmount / 256);
		tracks[i].decayPeriod = ((uint64_t)SAMPLE_CLOCK * 60 * tracks[i].decayAmount) / ((uint64_t)bpm * 4 * 256);

		//decayTuningWord = ((((double)bpm / 60) * 4) / ((double)decayAmount / 256)) * (double)POW_2_32 / SAMPLE_CLOCK;
		tracks[i].decayTuningWord = (bpm * ((uint64_t)POW_2_32 / 60) * 4 * 256 / tracks[i].decayAmount) / SAMPLE_CLOCK;
		
		tracks[i].decayPhaseRegister = 0;
		tracks[i].decayStop = 0;
		/*
		printf("decayAmount:\t%u\n", tracks[i].decayAmount);
		printf("decayPeriod:\t%u\n", tracks[i].decayPeriod);
		printf("decayTunigWord:\t%u\n", tracks[i].decayTuningWord);
		*/
	}

	for (int i = 0; i < period; i++)
	{
		tick++;

		if (tick >= ticksPerNote) {
			noteCount++;
			//printf("%d\t%d\n", tick, noteCount);

			// noteの先頭でtickをリセット
			tick = 0;

			// noteの先頭でwavePhaseRegister, decayPhaserRegisterをリセット
			for (int j = 0; j < TRACK_N; j++) {
				tracks[j].wavePhaseRegister = 0;
				tracks[j].decayPhaseRegister = 0;
				tracks[j].decayStop = 0;
			}
		}
		//printf("%d\t%d\t", noteCount, tick);

		// トラックの処理
		//
		for (int j = 0; j < TRACK_N; j++) {
			/*
			if (tracks[j].sequence[noteCount % SEQUENCE_LEN] == 0) {
				tracks[j].waveValue = int_to_fp32(0);
				//printf("\n");
				continue;
			}
			*/
			// Decayの処理 ***********************************************************
			//
			//***********************************************************************
			if (!tracks[j].decayStop) {
				tracks[j].decayPhaseRegister += tracks[j].decayTuningWord;
			}
			if (tick == tracks[j].decayPeriod - 1) {
				tracks[j].decayStop = 1;
			}

			// 32bitのphaseRegisterをテーブルの7bit(128個)に丸める
			int decayIndex = tracks[j].decayPhaseRegister >> 25;
			//printf("%d\t%d\t%d\t", tick, tracks[j].decayPhaseRegister, decayIndex);

			tracks[j].decayValue = *(tracks[j].decayLookupTable + decayIndex);
			//printf("%f\t", fp32_to_double(tracks[j].decayValue));

			// サンプル毎の振幅変調の合算 **********************************************
			//
			//************************************************************************ 
			fp32 amValue = tracks[j].decayValue;
			//printf("%f\t", fp32_to_double(amValue));

			// Wave系の処理 ***********************************************************
			//
			//************************************************************************
			switch (j) {
			case 0:	// kick
				tracks[j].waveValue = generateDDSWave(
					&(tracks[j].wavePhaseRegister),
					tracks[j].waveTuningWord,
					tracks[j].waveLookupTable);
				break;
			case 1:	// snare
				tracks[j].waveValue = generateDDSWave(
					&(tracks[j].wavePhaseRegister),
					tracks[j].waveTuningWord,
					tracks[j].waveLookupTable);
				break;
			case 2:	// hihat
				tracks[j].waveValue = generateNoise();
				break;
			default:
				fprintf(stderr, "Track no. out of range: %d\n", j);
			}			

			// 振幅変調 ***************************************************************
			// waveValue: -1.0 .. 1.0
			// amValue:    0.0 .. 1.0
			//************************************************************************
			tracks[j].waveValue = fp32_mul(tracks[j].waveValue, amValue);
			//printf("%f\t", fp32_to_double(tracks[j].waveValue));

			//printf("\n");
		}
		
		// トラックの合成 ***********************************************************
		//
		// ************************************************************************
		fp32 synthWaveValue = int_to_fp32(0);
		for (int i = 0; i < TRACK_N; i++) {
			fp32 fv;
			// 各トラックの出力値： waveValue * sequence[note](Velocity) * ampAmount
			fv = fp32_mul(tracks[i].waveValue, int_to_fp32(tracks[i].sequence[noteCount % SEQUENCE_LEN]));
			fv = fp32_mul(fv, int_to_fp32(tracks[i].ampAmount));
			fv = fp32_div(fv, int_to_fp32(UINT8_MAX));
			//printf("%f\t", fp32_to_double(fv));
			synthWaveValue = fp32_add(synthWaveValue, fv); 
		}
		//printf("%f\t", fp32_to_double(synthWaveValue));

		// 出力値の補正 ***********************************************************
		//
		// ************************************************************************
		
		// リミッター
		//
		if (synthWaveValue >= int_to_fp32(1))
			synthWaveValue = int_to_fp32(1);
		else if (synthWaveValue < int_to_fp32(-1))
			synthWaveValue = int_to_fp32(-1);

		// for 12bit output (0..4095)
		// 2048で乗算すると12bit幅を超えるため2047で乗算
		//
		fp32 fp32_12bit = fp32_mul(synthWaveValue + int_to_fp32(1), int_to_fp32(2047));
		int16_t i12v = fp32_to_int(fp32_12bit);
		//printf("%d\t", i12v);

		// for 12bit output (0..4095) as 16bit RAW format
		//
		printf("%d\t", (int)(i12v - 2048) << 4);

		// for 16bit output (-32768 .. 32767)
		//
		fp32 fp32_16bit = fp32_mul(synthWaveValue, int_to_fp32(32767));
		int16_t out_16bit = fp32_to_int(fp32_16bit);
		//printf("%d\t", out_16bit);
		//fwrite(&out_16bit, sizeof(out_16bit), 1, stdout);

		printf("\n");
	}	
	return 0;
}
