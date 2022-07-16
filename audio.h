typedef int32_t (*audio_callback)(int32_t frames_to_deliver, float *buf);

void playLoop(float nSeconds, uint32_t  samplesPerSecond, audio_callback func);
