#ifndef NANOLIVELENS_CORE_SOUND_HXX
#define NANOLIVELENS_CORE_SOUND_HXX

enum class Sound
{
    CLICK,
    LOGIN,
    LOGOUT,
	INFO,
    ERRR,
	EXTERNAL_SERVICE_CRASH,
    RELOAD
};

//void playSound(const stdf::path oggPath);
void initSound();
void deInitSound();
void playTestSound();
void playSound(const Sound sound);

#endif //NANOLIVELENS_CORE_SOUND_HXX
