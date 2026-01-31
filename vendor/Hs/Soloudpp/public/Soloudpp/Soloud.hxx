#ifndef HS_SOLOUDPP_HXX
#define HS_SOLOUDPP_HXX

#include <soloud.h>
#include <vector>
#include <span>
#include <stdexcept>

#include "soloud_wav.h"

namespace Holoop::Soloudpp
{

class InitErr : public std::runtime_error
{
public:
    InitErr(std::string_view &&msg);
};

class LoadAudioDataErr : public std::runtime_error
{
public:
    LoadAudioDataErr(std::string_view &&msg);
};

class Wav
{
public:
    void copyFromMem(const std::span<std::byte> audioFile);
    void loadFromMem(std::span<const std::byte> audioFile);
    void moveFromMem(std::vector<std::byte> &&audioFile);
    SoLoud::Wav &getRawWav();
    const SoLoud::Wav &getRawWavConst() const;
private:
    SoLoud::Wav wav_;
};

class Soloud
{
public:
    Soloud();
    Soloud(int sampleRate);
    ~Soloud();

    void play(Wav &wave);
private:
    SoLoud::Soloud soloud_;
};

}

#endif //HS_SOLOUDPP_HXX