#ifndef NANOLIVELENS_SOUND_HXX
#define NANOLIVELENS_SOUND_HXX

enum class Sound
{
    CLICK,
    LOGIN,
    LOGOUT,
    WARNN,
    ERRR,
    FATAL
};

//void playSound(const stdf::path oggPath);
void initSound();
void deInitSound();
void playTestSound();
void playSound(const Sound sound);

#endif //NANOLIVELENS_SOUND_HXX