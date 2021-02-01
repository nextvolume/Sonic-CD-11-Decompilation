#ifndef AUDIO_H
#define AUDIO_H

#include <stdlib.h>

#include <vorbis/vorbisfile.h>

#define ADJUST_VOLUME(s, v) (s = (s * v) / MAX_VOLUME)

#if RETRO_USING_SDL2
#include "SDL.h"
#define LOCK_AUDIO_DEVICE() SDL_LockAudio();
#define UNLOCK_AUDIO_DEVICE() SDL_UnlockAudio();
#else
#define LOCK_AUDIO_DEVICE() ;
#define UNLOCK_AUDIO_DEVICE() ;

#define AUDIO_FREQUENCY (44100)
#define AUDIO_SAMPLES   (0x800)

#endif

#if RETRO_USING_ALLEGRO4
#include <allegro.h>
#endif

#define TRACK_COUNT (0x10)
#define SFX_COUNT (0x100)
#define CHANNEL_COUNT (0x4)
#define SFXDATA_COUNT (0x400000)

#define MAX_VOLUME (100)

struct TrackInfo {
    char fileName[0x40];
    bool trackLoop;
    uint loopPoint;
};

struct MusicPlaybackInfo {
#if !RETRO_DISABLE_OGGVORBIS
    int vorbBitstream;
    OggVorbis_File vorbisFile;
#endif
	
#if RETRO_USING_SDL2
    SDL_AudioStream *stream;
    Sint16 *buffer;
#endif

#if RETRO_USING_ALLEGRO4
    AUDIOSTREAM *stream;
#endif

    FileInfo fileInfo;
    bool trackLoop;
    uint loopPoint;
    bool loaded;
};

struct SFXInfo {
    char name[0x40];
    Sint16 *buffer;
    size_t length;
    bool loaded;
};

struct ChannelInfo {
    size_t sampleLength;
    Sint16 *samplePtr;
    int sfxID;
    byte loopSFX;
    sbyte pan;
};

enum MusicStatuses {
    MUSIC_STOPPED = 0,
    MUSIC_PLAYING = 1,
    MUSIC_PAUSED  = 2,
    MUSIC_LOADING = 3,
    MUSIC_READY   = 4,
};

extern int globalSFXCount;
extern int stageSFXCount;

extern int masterVolume;
extern int trackID;
extern int sfxVolume;
extern int bgmVolume;
extern bool audioEnabled;

extern int nextChannelPos;
extern bool musicEnabled;
extern int musicStatus;
extern TrackInfo musicTracks[TRACK_COUNT];
extern SFXInfo sfxList[SFX_COUNT];

extern ChannelInfo sfxChannels[CHANNEL_COUNT];

extern MusicPlaybackInfo musInfo;

#if RETRO_USING_SDL2
extern SDL_AudioSpec audioDeviceFormat;
#endif

int InitAudioPlayback();
void ProcessAudioPlayback(void *data, Uint8 *stream, int len);

#if RETRO_USING_SDL2

void ProcessMusicStream(void *data, Sint16 *stream, int len);
void ProcessAudioMixing(Sint32 *dst, const Sint16 *src, int len, int volume, sbyte pan);

#endif

inline void freeMusInfo()
{
    if (musInfo.loaded) {
        LOCK_AUDIO_DEVICE()

#if RETRO_USING_SDL2
        if (musInfo.buffer)
            delete[] musInfo.buffer;
	musInfo.buffer    = nullptr;

        if (musInfo.stream)
            SDL_FreeAudioStream(musInfo.stream);
#endif
#if !RETRO_DISABLE_OGGVORBIS
        ov_clear(&musInfo.vorbisFile);
#endif	
#if RETRO_USING_SDL2
        musInfo.stream = nullptr;
#endif
        musInfo.trackLoop = false;
        musInfo.loopPoint = 0;
        musInfo.loaded    = false;

        UNLOCK_AUDIO_DEVICE()
    }
}


void SetMusicTrack(char *filePath, byte trackID, bool loop, uint loopPoint);
bool PlayMusic(int track);
inline void StopMusic()
{
    musicStatus = MUSIC_STOPPED;
    freeMusInfo();
}

void LoadSfx(char *filePath, byte sfxID);
void PlaySfx(int sfx, bool loop);
inline void StopSfx(int sfx)
{
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID == sfx) {
            MEM_ZERO(sfxChannels[i]);
            sfxChannels[i].sfxID = -1;
        }
    }
}
void SetSfxAttributes(int sfx, int loopCount, sbyte pan);

inline void SetMusicVolume(int volume)
{
    if (volume < 0)
        volume = 0;
    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;
    masterVolume = volume;
}

inline void PauseSound()
{
    if (musicStatus == MUSIC_PLAYING)
        musicStatus = MUSIC_PAUSED;
}

inline void ResumeSound()
{
    if (musicStatus == MUSIC_PAUSED)
        musicStatus = MUSIC_PLAYING;
}


inline void StopAllSfx()
{
    for (int i = 0; i < CHANNEL_COUNT; ++i) sfxChannels[i].sfxID = -1;
}
inline void ReleaseGlobalSfx()
{
    for (int i = globalSFXCount; i >= 0; --i) {
        if (sfxList[i].loaded) {
            StrCopy(sfxList[i].name, "");
            free(sfxList[i].buffer);
            sfxList[i].length = 0;
            sfxList[i].loaded = false;
        }
    }
    globalSFXCount = 0;
}
inline void ReleaseStageSfx()
{
    for (int i = stageSFXCount + globalSFXCount; i >= globalSFXCount; --i) {
        if (sfxList[i].loaded) {
            StrCopy(sfxList[i].name, "");
            free(sfxList[i].buffer);
            sfxList[i].length = 0;
            sfxList[i].loaded = false;
        }
    }
    stageSFXCount = 0;
}

inline void ReleaseAudioDevice()
{
    StopMusic();
    StopAllSfx();
    ReleaseStageSfx();
    ReleaseGlobalSfx();
}

#endif // !AUDIO_H
