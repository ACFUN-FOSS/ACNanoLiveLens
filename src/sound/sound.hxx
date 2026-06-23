#ifndef NANOLIVELENS_SOUND_HXX
#define NANOLIVELENS_SOUND_HXX

enum class Sound
{
    CLICK,
    LOGIN,
    LOGOUT,
	INFO,
    WARNN,
    ERRR,
    RELOAD
};

//void playSound(const stdf::path oggPath);
void initSound();
void deInitSound();
void playTestSound();
void playSound(const Sound sound);

#endif //NANOLIVELENS_SOUND_HXX
