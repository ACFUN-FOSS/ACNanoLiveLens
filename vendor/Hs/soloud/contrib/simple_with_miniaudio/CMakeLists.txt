cmake_minimum_required (VERSION 3.8)
project(soloud)

if(${PROJECT_IS_TOP_LEVEL})
	message(FATAL_ERROR "This project can be used as a subproject only (via add_subdirectory).")
endif ()

add_library(soloud STATIC)
add_library(soloud::soloud ALIAS soloud)

set(HEADER_PATH ../../include)
set(SOURCE_PATH ../../src)

# Core
set(CORE_PATH ${SOURCE_PATH}/core)
set(CORE_SOURCES
	${CORE_PATH}/soloud.cpp
	${CORE_PATH}/soloud_audiosource.cpp
	${CORE_PATH}/soloud_bus.cpp
	${CORE_PATH}/soloud_core_3d.cpp
	${CORE_PATH}/soloud_core_basicops.cpp
	${CORE_PATH}/soloud_core_faderops.cpp
	${CORE_PATH}/soloud_core_filterops.cpp
	${CORE_PATH}/soloud_core_getters.cpp
	${CORE_PATH}/soloud_core_setters.cpp
	${CORE_PATH}/soloud_core_voicegroup.cpp
	${CORE_PATH}/soloud_core_voiceops.cpp
	${CORE_PATH}/soloud_fader.cpp
	${CORE_PATH}/soloud_fft.cpp
	${CORE_PATH}/soloud_fft_lut.cpp
	${CORE_PATH}/soloud_misc.cpp
	${CORE_PATH}/soloud_queue.cpp
	${CORE_PATH}/soloud_file.cpp
	${CORE_PATH}/soloud_filter.cpp
	${CORE_PATH}/soloud_thread.cpp
)


# Audiosources
set(AUDIOSOURCES_PATH ${SOURCE_PATH}/audiosource)
set(AUDIOSOURCES_SOURCES
	# ay
	${AUDIOSOURCES_PATH}/ay/chipplayer.cpp
	${AUDIOSOURCES_PATH}/ay/chipplayer.h
	${AUDIOSOURCES_PATH}/ay/readme.txt
	${AUDIOSOURCES_PATH}/ay/sndbuffer.cpp
	${AUDIOSOURCES_PATH}/ay/sndbuffer.h
	${AUDIOSOURCES_PATH}/ay/sndchip.cpp
	${AUDIOSOURCES_PATH}/ay/sndchip.h
	${AUDIOSOURCES_PATH}/ay/sndrender.cpp
	${AUDIOSOURCES_PATH}/ay/sndrender.h
	${AUDIOSOURCES_PATH}/ay/soloud_ay.cpp

	# monotone
	${AUDIOSOURCES_PATH}/monotone/soloud_monotone.cpp

	# noise
	${AUDIOSOURCES_PATH}/noise/soloud_noise.cpp

	# openmpt
	${AUDIOSOURCES_PATH}/openmpt/soloud_openmpt.cpp
	${AUDIOSOURCES_PATH}/openmpt/soloud_openmpt_dll.c

	# sfxr
	${AUDIOSOURCES_PATH}/sfxr/soloud_sfxr.cpp

	# speech
	${AUDIOSOURCES_PATH}/speech/Elements.def
	${AUDIOSOURCES_PATH}/speech/darray.cpp
	${AUDIOSOURCES_PATH}/speech/darray.h
	${AUDIOSOURCES_PATH}/speech/klatt.cpp
	${AUDIOSOURCES_PATH}/speech/klatt.h
	${AUDIOSOURCES_PATH}/speech/resonator.cpp
	${AUDIOSOURCES_PATH}/speech/resonator.h
	${AUDIOSOURCES_PATH}/speech/soloud_speech.cpp
	${AUDIOSOURCES_PATH}/speech/tts.cpp
	${AUDIOSOURCES_PATH}/speech/tts.h

	# tedsid
	${AUDIOSOURCES_PATH}/tedsid/sid.cpp
	${AUDIOSOURCES_PATH}/tedsid/sid.h
	${AUDIOSOURCES_PATH}/tedsid/soloud_tedsid.cpp
	${AUDIOSOURCES_PATH}/tedsid/ted.cpp
	${AUDIOSOURCES_PATH}/tedsid/ted.h

	# vic
	${AUDIOSOURCES_PATH}/vic/soloud_vic.cpp

	# vizsn
	${AUDIOSOURCES_PATH}/vizsn/soloud_vizsn.cpp

	# wav
	${AUDIOSOURCES_PATH}/wav/dr_flac.h
	${AUDIOSOURCES_PATH}/wav/dr_impl.cpp
	${AUDIOSOURCES_PATH}/wav/dr_mp3.h
	${AUDIOSOURCES_PATH}/wav/dr_wav.h
	${AUDIOSOURCES_PATH}/wav/soloud_wav.cpp
	${AUDIOSOURCES_PATH}/wav/soloud_wavstream.cpp
	${AUDIOSOURCES_PATH}/wav/stb_vorbis.c
	${AUDIOSOURCES_PATH}/wav/stb_vorbis.h
)

set(BACKENDS_PATH "${SOURCE_PATH}/backend")

target_compile_definitions(soloud PRIVATE -DWITH_MINIAUDIO)
set(BACKENDS_SOURCES
	${BACKENDS_PATH}/miniaudio/soloud_miniaudio.cpp
)
target_include_directories(soloud PUBLIC ${HEADER_PATH})
target_link_libraries(soloud PRIVATE ${SOLOUD_DEP_MINIAUDIO_TARGET})

target_sources(soloud 
	PRIVATE
	${CORE_SOURCES}
	${AUDIOSOURCES_SOURCES}
	${BACKENDS_SOURCES}
	${FILTERS_SOURCES}
)
