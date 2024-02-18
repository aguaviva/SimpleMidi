typedef int32_t (*audio_callback)(int32_t frames_to_deliver, const void *object, float *buf);

void playLoop(float nSeconds, uint32_t  samplesPerSecond, const void *object, audio_callback func);
