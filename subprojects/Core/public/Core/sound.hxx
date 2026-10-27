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

void initSound();
void deInitSound();
void playTestSound();
void playSound(const Sound sound);

// 播放 assets 目录下的一个音频文件。oggName 是相对 assets 目录的路径，
// 例如 "scripts/danmaku-highlight.ogg"；支持 ogg / wav / flac / mp3（按内容识别）。
// 同一个路径只在首次播放时读盘并解码，之后重复播放复用缓存。
// 文件缺失或解码失败时抛异常。
void playOgg(std::string_view oggName);

#endif //NANOLIVELENS_CORE_SOUND_HXX
