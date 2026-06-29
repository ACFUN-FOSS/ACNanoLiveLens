
using namespace Holoop;
using namespace Essentials::IO;
#include "sound.hxx"
#include "Core/assets.hxx"

static Soloudpp::Soloud &soloud() {
    static Soloudpp::Soloud soloud;
    return soloud;
}

void initSound() {
    soloud();
}

static void playOgg(std::string_view oggName) {
    static std::unordered_map<std::string, Soloudpp::Wav> oggNameToWav;

    std::string oggNameStr{ oggName };
    if (!oggNameToWav.contains(oggNameStr)) {
        auto oggPath = getAssetsDir() / oggName;
        auto oggData = Essentials::IO::readFileRaw(oggPath);
        auto &wav = oggNameToWav[oggNameStr];
        wav.loadFromMem(oggData);
    }

    auto &wav = oggNameToWav[oggNameStr];
    soloud().play(wav);
}

void playSound(const Sound sound) {
    switch (sound) {
        case Sound::ERRR:
            playOgg("information_2.ogg");
            break;
        case Sound::RELOAD:
            playOgg("hot_reload.ogg");
            break;
        default:
            break;
    }
}

void deInitSound() {
    
}
