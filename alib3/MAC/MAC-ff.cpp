/** libMAC interface
2016, Simon Zolin */

#include "MAC-ff.h"
#include <All.h>
#include <APEDecompress.h>
#include <NewPredictor.h>
#include <APEInfo.h>

static const char *const errs[] = {
	"unsupported version",
	"bad data",
	"CRC mismatch",
	"need more data",
};

const char* ape_errstr(int e)
{
	e = -e - 1;
	return errs[e];
}


struct ape_decoder {
	APE::CAPEDecompress *dec;
};

/* MACLib/MACLib.cpp */
static void _FillWaveFormatEx(APE::WAVEFORMATEX * pWaveFormatEx, int nSampleRate, int nBitsPerSample, int nChannels)
{
	pWaveFormatEx->cbSize = 0;
	pWaveFormatEx->nSamplesPerSec = nSampleRate;
	pWaveFormatEx->wBitsPerSample = nBitsPerSample;
	pWaveFormatEx->nChannels = nChannels;
	pWaveFormatEx->wFormatTag = 1;

	pWaveFormatEx->nBlockAlign = (pWaveFormatEx->wBitsPerSample / 8) * pWaveFormatEx->nChannels;
	pWaveFormatEx->nAvgBytesPerSec = pWaveFormatEx->nBlockAlign * pWaveFormatEx->nSamplesPerSec;
}

int ape_decode_init(ape_decoder **pa, const struct ape_info *info)
{
	int r = 0;
	ape_decoder *a = new ape_decoder();

	if (info->version >= 3930) {
		a->dec = new APE::CAPEDecompress;
		a->dec->version = info->version;
		a->dec->m_nBlockAlign = info->bitspersample / 8 * info->channels;
		_FillWaveFormatEx(&a->dec->m_wfeInput, info->samplerate, info->bitspersample, info->channels);

		a->dec->m_spUnBitArray.Assign(APE::CreateUnBitArray(NULL, info->version));
		if (a->dec->m_spUnBitArray == NULL) {
			ape_decode_free(a);
			return -APE_EVERSION;
		}

		if (info->version >= 3950) {
			a->dec->m_spNewPredictorX.Assign(new APE::CPredictorDecompress3950toCurrent(info->compressionlevel, info->version));
			a->dec->m_spNewPredictorY.Assign(new APE::CPredictorDecompress3950toCurrent(info->compressionlevel, info->version));
		} else {
			a->dec->m_spNewPredictorX.Assign(new APE::CPredictorDecompressNormal3930to3950(info->compressionlevel, info->version));
			a->dec->m_spNewPredictorY.Assign(new APE::CPredictorDecompressNormal3930to3950(info->compressionlevel, info->version));
		}

	} else
		return -APE_EVERSION;

	*pa = a;
	return 0;
}

void ape_decode_free(ape_decoder *a)
{
	delete a->dec;
	delete a;
}

int ape_decode(ape_decoder *a, const char *data, size_t len, char *pcm, unsigned int samples, unsigned int align4)
{
	int r;

	a->dec->m_spUnBitArray->m_pBitArray = (unsigned int*)data;
	a->dec->m_spUnBitArray->m_nBits = len * 8;
	a->dec->m_spUnBitArray->m_nGoodBytes = len;
	a->dec->m_spUnBitArray->m_nCurrentBitIndex = align4 * 8;

	a->dec->pcm = (unsigned char*)pcm;

	try {
		a->dec->StartFrame();
		a->dec->DecodeBlocksToFrameBuffer(samples);

	} catch(int i) {
		if (i == APE_EMOREDATA)
			return -APE_EMOREDATA;
		return -APE_EDATA;

	} catch(...) {
		return -APE_EDATA;
	}

	a->dec->m_nCRC ^= 0xffffffff;
	a->dec->m_nCRC >>= 1;
	if (a->dec->m_nCRC != a->dec->m_nStoredCRC)
		return -APE_ECRC;

	return (a->dec->pcm - (unsigned char*)pcm) / a->dec->m_nBlockAlign;
}


#include <CharacterHelper.h>

APE::str_utf8 * APE::CAPECharacterHelper::GetUTF8FromUTF16(const APE::str_utfn * pUTF16){return NULL;}

int __stdcall FillWaveHeader(APE::WAVE_HEADER * pWAVHeader, APE::intn nAudioBytes, APE::WAVEFORMATEX * pWaveFormatEx, APE::intn nTerminatingBytes){return 0;}

APE::intn APE::CAPEInfo::GetInfo(APE::APE_DECOMPRESS_FIELDS, APE::intn, APE::intn){return 0;}

APE::CCircleBuffer::CCircleBuffer(){}
APE::CCircleBuffer::~CCircleBuffer(){}
void APE::CCircleBuffer::CreateBuffer(APE::intn nBytes, APE::intn nMaxDirectWriteBytes){}
APE::intn APE::CCircleBuffer::MaxAdd(){return 0;}
APE::intn APE::CCircleBuffer::MaxGet(){return 0;}
APE::intn APE::CCircleBuffer::Get(unsigned char * pBuffer, APE::intn nBytes){return 0;}
void APE::CCircleBuffer::Empty(){}
APE::intn APE::CCircleBuffer::RemoveHead(APE::intn nBytes){return 0;}
APE::intn APE::CCircleBuffer::RemoveTail(APE::intn nBytes){return 0;}
APE::CAPETag::~CAPETag(){}
APE::CAPEInfo::~CAPEInfo(){}
