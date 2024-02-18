#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "misc.h"

bool load_file(const char* filename, uint8_t** pOut, size_t* pSize)
{
    FILE* f = fopen(filename, "rb");
    if (f == NULL)
    {
        printf("Can't open %s\n", filename);
        return false;
    }

    fseek(f, 0L, SEEK_END);
    *pSize = ftell(f);
    fseek(f, 0L, SEEK_SET);

    *pOut = (uint8_t*)malloc(*pSize);

    fread(*pOut, *pSize, 1, f);
    fclose(f);

    return true;
}

float unlerp(int i, int ini, int fin)
{
    return (float)(i - ini) / (float)(fin - ini);
}

float unlerp(float i, float ini, float fin)
{
    return (float)(i - ini) / (float)(fin - ini);
}

int lerp(float t, int out_ini, int out_fin)
{
    return t*(float)out_fin + (1.0f-t)*(float)out_ini;
}

float lerp(float t, float out_ini, float out_fin)
{
    return t*(float)out_fin + (1.0f-t)*(float)out_ini;
}

int map(int i, int ini, int fin, int out_ini, int out_fin)
{
    float t = unlerp(i, ini, fin);
    return lerp(t, out_ini, out_fin);
}

#include <dirent.h>
#include <string>
#include <vector>

int get_midi_list(const char *pDir, std::vector<std::string> *pMidi_files)
{
  DIR *dp;
  struct dirent *ep;     
  dp = opendir (pDir);
  if (dp != NULL)
  {
    while ((ep = readdir (dp)) != NULL)
    {
        std::string str = (ep->d_name);
        if (str.size()>3)
        {
            std::string ext = str.substr(str.size()-4,4);
            if ( ext == ".mid")
                pMidi_files->push_back(str);
        }
    }
          
    closedir (dp);
    return 0;
  }
  else
  {
    perror ("Couldn't open the directory");
    return -1;
  }
}
