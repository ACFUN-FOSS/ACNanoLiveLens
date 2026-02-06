#include "sound.hxx"
#include <cstddef>
#include <gsl/gsl>
#include <span>
#include <Soloudpp/Soloud.hxx>
#include <../../vendor/Eati/Essentials/public/EatiEssentials/container_and_view_and_ranges/container_and_view_and_range.hxx>

using namespace Holoop;
using namespace Essentials::ContainerAndView;


static void deInit() {

}

static Soloudpp::Wav &fatalWave() {
    static bool loaded = false;

    static Soloudpp::Wav wav;
    //if (!loaded)
    //    wav.loadFromMem(std::span{ getFatalOggData() });
    return wav;
}

static Soloudpp::Soloud &soloud() {
    static Soloudpp::Soloud soloud;
    return soloud;
}

void initSound() {
    soloud();
    fatalWave();
}

void playOgg(std::span<const std::byte> oggData) {
    soloud().play(fatalWave());
}

void playTestSound() {
    //playOgg(getFatalOggData());
}

void deInitSound() {
    deInit();
}