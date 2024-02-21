#include <vector>
#include <string>
#include <stdio.h>

bool load_file(const char* filename, uint8_t** pOut, size_t* pSize);

int map(int i, int ini, int fin, int out_ini, int out_fin);

float unlerp(int i, int ini, int fin);
float unlerp(float i, float ini, float fin);
int lerp(float t, int out_ini, int out_fin);
float lerp(float t, float out_ini, float out_fin);

int get_midi_list(const char *pDir, std::vector<std::string> *pMidi_files);

#define LOG(...)

