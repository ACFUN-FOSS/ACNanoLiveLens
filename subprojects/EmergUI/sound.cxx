#include "sound.hxx"
#include <array>
#include <Soloudpp/Soloud.hxx>

extern const std::array<std::byte, 9741> fatalOggData;

void crashSound() {
    static Holoop::Soloudpp::Soloud soloud;
    static Holoop::Soloudpp::Wav wav;
    wav.loadFromMem(std::span{ fatalOggData });
    soloud.play(wav);
}
