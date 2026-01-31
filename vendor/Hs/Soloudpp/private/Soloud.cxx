#include "Soloudpp/Soloud.hxx"


using namespace std::string_literals;

namespace Holoop::Soloudpp
{

InitErr::InitErr(std::string_view &&msg)
    : std::runtime_error{ "SoLoud init failed: "s + msg.data() } { }

LoadAudioDataErr::LoadAudioDataErr(std::string_view &&msg)
    : std::runtime_error{ "Load audio data failed: "s + msg.data() } { }

static std::string_view result2String(const SoLoud::result result) {
    // This is OK, as the constructor does in fact nothing.
    SoLoud::Soloud soloud;
    return soloud.getErrorString(result);
}

static void ensureLoadSucc(const SoLoud::result result) {
    if (result != 0)
        throw LoadAudioDataErr{ result2String(result) };
}

void Wav::copyFromMem(const std::span<std::byte> audioFile) {
    auto result = wav_.loadMem(
        reinterpret_cast<const unsigned char *>(audioFile.data()),
        audioFile.size(),
        true,
        false
    );
    ensureLoadSucc(result);
}

void Wav::loadFromMem(const std::span<const std::byte> audioFile) {
    auto result = wav_.loadMem(
        reinterpret_cast<const unsigned char *>(audioFile.data()),
        audioFile.size(),
        false,
        false
    );
    ensureLoadSucc(result);
}

void Wav::moveFromMem(std::vector<std::byte> &&audioFile) {
    auto result = wav_.loadMem(
        reinterpret_cast<const unsigned char *>(audioFile.data()),
        audioFile.size(),
        false,
        true
    );
    ensureLoadSucc(result);
}

SoLoud::Wav &Wav::getRawWav() {
    return wav_;
}

const SoLoud::Wav & Wav::getRawWavConst() const {
    return wav_;
}

static void ensureInitSucc(const SoLoud::result result) {
    if (result != 0)
        throw InitErr{ result2String(result) };
}

Soloud::Soloud() {
    auto result = soloud_.init();
    ensureInitSucc(result);
}
Soloud::Soloud(int sampleRate) {
    auto result = soloud_.init(
        SoLoud::Soloud::CLIP_ROUNDOFF,
        SoLoud::Soloud::AUTO,
        sampleRate,
        2
    );
    ensureInitSucc(result);
}

Soloud::~Soloud() {
    soloud_.deinit();
}

void Soloud::play(Wav &wave) {
    soloud_.play(wave.getRawWav());
}
}
