#include <fstream>
#include "userosc.h"

#define BLOCK_SIZE 64

user_osc_param_t param;

void init()
{
	OSC_INIT(0, 0);
	for (uint16_t p = 0; p < k_num_user_osc_param_id; p++) {
		OSC_PARAM(p, 0);
	}
	param.pitch = 0;
	param.shape_lfo = 0;
	param.cutoff = 0;
	param.resonance = 0;
}

void note_on(uint16_t pitch)
{
	param.pitch = pitch;
	OSC_NOTEON(&param);
}

void note_off()
{
	param.pitch = 0;
	param.shape_lfo = 0;
	OSC_NOTEOFF(&param);
}

void generate(int32_t* buffer, uint32_t nsamples)
{
	uint32_t n = 0;
	while (n + BLOCK_SIZE <= nsamples) {
		// TODO: shape LFO
		OSC_CYCLE(&param, &buffer[n], BLOCK_SIZE);
		n += BLOCK_SIZE;
	}
}

void set_wavetable(uint16_t wt)
{
	OSC_PARAM(k_user_osc_param_id1, wt);
}

void set_wave(float wave)
{
	uint32_t nwave = (uint32_t)(wave * 33554432.f);
	OSC_PARAM(k_user_osc_param_shape, static_cast<uint16_t>((nwave >> 22) & 0x3ff));
	OSC_PARAM(k_user_osc_param_shiftshape, static_cast<uint16_t>((nwave >> 12) & 0x3ff));
}

void set_env_attack(uint16_t env_a)
{
	OSC_PARAM(k_user_osc_param_id2, env_a);
}

void set_env_decay(uint16_t env_d)
{
	OSC_PARAM(k_user_osc_param_id3, env_d);
}

void set_env_amount(int16_t env_amount)
{
	// -99 to 100
	OSC_PARAM(k_user_osc_param_id3, (uint16_t)(env_amount + 100));
}


int main(int argc, char* argv[])
{
	int16_t wavetable = 0;
	float wave = 48;
	uint32_t nsamples = 512;
	if (argc > 1) {
		wavetable = atoi(argv[1]);
	}
	if (argc > 2) {
		wave = static_cast<float>(atof(argv[2]));
	}
	if (argc > 3) {
		nsamples = atoi(argv[3]);
	}

	init();
	set_wavetable(wavetable);
	set_wave(wave);

	int32_t* samples = new int32_t[nsamples];

	generate(samples, nsamples);

	if (1) {
		std::ofstream fout("res.bin", std::ofstream::binary);
		fout.write((const char*)samples, nsamples * sizeof(int32_t));
		fout.close();
	}

	delete[] samples;
}

