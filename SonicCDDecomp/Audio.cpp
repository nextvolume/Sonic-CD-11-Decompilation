#include "RetroEngine.hpp"
#include <cmath>
#include <iostream>

int globalSFXCount = 0;
int stageSFXCount  = 0;

int masterVolume  = MAX_VOLUME;
int trackID       = -1;
int sfxVolume     = MAX_VOLUME;
int bgmVolume     = MAX_VOLUME;
bool audioEnabled = false;

int nextChannelPos;
bool musicEnabled;
int musicStatus;
TrackInfo musicTracks[TRACK_COUNT];
SFXInfo sfxList[SFX_COUNT];

ChannelInfo sfxChannels[CHANNEL_COUNT];

MusicPlaybackInfo musInfo;

int trackBuffer = -1;

#if RETRO_USING_SDL2 && !RETRO_USING_SDL1
SDL_AudioDeviceID audioDevice;
SDL_AudioSpec audioDeviceFormat;
SDL_AudioStream *ogv_stream;

#define LOCK_AUDIO_DEVICE()   SDL_LockAudio();
#define UNLOCK_AUDIO_DEVICE() SDL_UnlockAudio();

#define AUDIO_FREQUENCY (44100)
#define AUDIO_FORMAT    (AUDIO_S16SYS) /**< Signed 16-bit samples */
#define AUDIO_SAMPLES   (0x800)
#define AUDIO_CHANNELS  (2)

#define ADJUST_VOLUME(s, v) (s = (s * v) / MAX_VOLUME)

#else
#define LOCK_AUDIO_DEVICE()   ;
#define UNLOCK_AUDIO_DEVICE() ;
#endif

#define MIX_BUFFER_SAMPLES (256)

#if !RETRO_USING_SDL2
static Sint16 *WavDataToBuffer(void *data, int num_frames, int num_channels,
		int bit_depth);
#endif

int InitAudioPlayback()
{
#if !RETRO_DISABLE_AUDIO
    StopAllSfx(); //"init"
#if RETRO_USING_SDL2
    SDL_AudioSpec want;
    want.freq     = AUDIO_FREQUENCY;
    want.format   = AUDIO_FORMAT;
    want.samples  = AUDIO_SAMPLES;
    want.channels = AUDIO_CHANNELS;
    want.callback = ProcessAudioPlayback;

    if ((audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &audioDeviceFormat, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE)) > 0) {
        audioEnabled = true;
        SDL_PauseAudioDevice(audioDevice, 0);
    }
    else {
        printLog("Unable to open audio device: %s", SDL_GetError());
        audioEnabled = false;
        return true; // no audio but game wont crash now
    }

    // Init video sound stuff
    // TODO: Unfortunately, we're assuming that video sound is stereo at 48000Hz.
    // This is true of every .ogv file in the game (the Steam version, at least),
    // but it would be nice to make this dynamic. Unfortunately, THEORAPLAY's API
    // makes this awkward.
    ogv_stream = SDL_NewAudioStream(AUDIO_F32SYS, 2, 48000, audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
    if (!ogv_stream) {
        printLog("Failed to create stream: %s", SDL_GetError());
        SDL_CloseAudioDevice(audioDevice);
        audioEnabled = false;
        return true; // no audio but game wont crash now
    }

#endif

#if RETRO_USING_ALLEGRO4
    if (install_sound(DIGI_AUTODETECT, 0, NULL) == -1) {
        printLog("Unable to open audio device: %s", allegro_error);
        audioEnabled = false;
        return true;
    }
    
    musInfo.stream = play_audio_stream(AUDIO_SAMPLES, 16, 1, AUDIO_FREQUENCY, 255, 127);
    
    audioEnabled = true;
#endif

    FileInfo info;
    FileInfo infoStore;
    char strBuffer[0x100];
    byte fileBuffer  = 0;
    int fileBuffer2 = 0;

    if (LoadFile("Data/Game/Gameconfig.bin", &info)) {
        infoStore = info;

        FileRead(&fileBuffer, 1);
        FileRead(strBuffer, fileBuffer);
        strBuffer[fileBuffer] = 0;

        FileRead(&fileBuffer, 1);
        FileRead(&strBuffer, fileBuffer); // Load 'Data'
        strBuffer[fileBuffer] = 0;

        FileRead(&fileBuffer, 1);
        FileRead(strBuffer, fileBuffer);
        strBuffer[fileBuffer] = 0;

        // Read Obect Names
        byte objectCount = 0;
        FileRead(&objectCount, 1);
        for (byte o = 0; o < objectCount; ++o) {
            FileRead(&fileBuffer, 1);
            FileRead(strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;
        }

        // Read Script Paths
        for (byte s = 0; s < objectCount; ++s) {
            FileRead(&fileBuffer, 1);
            FileRead(strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;
        }

        byte varCnt = 0;
        FileRead(&varCnt, 1);
        for (byte v = 0; v < varCnt; ++v) {
            FileRead(&fileBuffer, 1);
            FileRead(strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;

            // Read Variable Value
            FileRead(&fileBuffer2, 4);
        }

        // Read SFX
        FileRead(&fileBuffer, 1);
        globalSFXCount = fileBuffer;
        for (byte s = 0; s < globalSFXCount; ++s) {
            FileRead(&fileBuffer, 1);
            FileRead(strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;

            GetFileInfo(&infoStore);
            LoadSfx(strBuffer, s);
            SetFileInfo(&infoStore);
        }

        CloseFile();
    }

    // sfxDataPosStage = sfxDataPos;
    nextChannelPos = 0;
    for (int i = 0; i < CHANNEL_COUNT; ++i) sfxChannels[i].sfxID = -1;
#endif
    return true;

}

#if !RETRO_DISABLE_OGGVORBIS
size_t readVorbis(void *mem, size_t size, size_t nmemb, void *ptr)
{
    MusicPlaybackInfo *info = (MusicPlaybackInfo *)ptr;
    return FileRead2(&info->fileInfo, mem, (int)(size * nmemb));
}
int seekVorbis(void *ptr, ogg_int64_t offset, int whence)
{
    MusicPlaybackInfo *info = (MusicPlaybackInfo *)ptr;
    switch (whence) {
        case SEEK_SET: whence = 0; break;
        case SEEK_CUR: whence = (int)GetFilePosition2(&info->fileInfo); break;
        case SEEK_END: whence = info->fileInfo.fileSize; break;
        default: break;
    }
    SetFilePosition2(&info->fileInfo, (int)(whence + offset));
    return GetFilePosition2(&info->fileInfo) <= info->fileInfo.fileSize;
}
long tellVorbis(void *ptr)
{
    MusicPlaybackInfo *info = (MusicPlaybackInfo *)ptr;
    return GetFilePosition2(&info->fileInfo);
}
int closeVorbis(void *ptr) { return CloseFile2(); }
#endif

#if RETRO_USING_SDL2 || RETRO_USING_SDL1
void ProcessMusicStream(Sint32 *stream, size_t bytes_wanted)
{
#if !RETRO_DISABLE_AUDIO
    if (!musInfo.loaded)
        return;
    switch (musicStatus) {
        case MUSIC_READY:
        case MUSIC_PLAYING: {
#if RETRO_USING_SDL2
            while (SDL_AudioStreamAvailable(musInfo.stream) < bytes_wanted) {
                // We need more samples: get some
                long bytes_read = ov_read(&musInfo.vorbisFile, (char *)musInfo.buffer, sizeof(musInfo.buffer), 0, 2, 1, &musInfo.vorbBitstream);

                if (bytes_read == 0) {
                    // We've reached the end of the file
                    if (musInfo.trackLoop) {
                        ov_pcm_seek(&musInfo.vorbisFile, musInfo.loopPoint);
                        continue;
                    }
                    else {
                        musicStatus = MUSIC_STOPPED;
                        break;
                    }
                }

                if (SDL_AudioStreamPut(musInfo.stream, musInfo.buffer, bytes_read) == -1)
                    return;
            }

            // Now that we know there are enough samples, read them and mix them
            int bytes_done = SDL_AudioStreamGet(musInfo.stream, musInfo.buffer, bytes_wanted);
            if (bytes_done == -1) {
                return;
            }
            if (bytes_done != 0)
                ProcessAudioMixing(stream, musInfo.buffer, bytes_done / sizeof(Sint16), (bgmVolume * masterVolume) / MAX_VOLUME, 0);
#endif
        } break;
        case MUSIC_STOPPED:
        case MUSIC_PAUSED:
        case MUSIC_LOADING:
            // dont play
            break;
    }
}
#endif
#endif

void ProcessAudioPlayback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata; // Unused
#if !RETRO_DISABLE_AUDIO
    if (!audioEnabled)
        return;

    if (musicStatus == MUSIC_LOADING) {
        if (trackBuffer < 0 || trackBuffer >= TRACK_COUNT) {
            StopMusic();
            return;
        }

        TrackInfo *trackPtr = &musicTracks[trackBuffer];

        if (!trackPtr->fileName[0]) {
            StopMusic();
            return;
        }

        if (musInfo.loaded)
            StopMusic();

        if (LoadFile(trackPtr->fileName, &musInfo.fileInfo)) {
            cFileHandleStream = cFileHandle;
            cFileHandle                  = nullptr;

            musInfo.trackLoop = trackPtr->trackLoop;
            musInfo.loopPoint = trackPtr->loopPoint;
            musInfo.loaded    = true;

            unsigned long long samples = 0;
#if !RETRO_DISABLE_OGGVORBIS
            ov_callbacks callbacks;

            callbacks.read_func  = readVorbis;
            callbacks.seek_func  = seekVorbis;
            callbacks.tell_func  = tellVorbis;
            callbacks.close_func = closeVorbis;

            int error = ov_open_callbacks(&musInfo, &musInfo.vorbisFile, NULL, 0, callbacks);
            if (error != 0) {
            }

            musInfo.vorbBitstream = -1;
            musInfo.vorbisFile.vi = ov_info(&musInfo.vorbisFile, -1);
#endif
	    
#if RETRO_USING_SDL2
            musInfo.stream = SDL_NewAudioStream(AUDIO_S16, musInfo.vorbisFile.vi->channels, musInfo.vorbisFile.vi->rate, audioDeviceFormat.format,
                                                audioDeviceFormat.channels, audioDeviceFormat.freq);
            if (!musInfo.stream) {
                printLog("Failed to create stream: %s", SDL_GetError());
            }
	    
	    musInfo.buffer = new Sint16[MIX_BUFFER_SAMPLES];
#endif

            musicStatus  = MUSIC_PLAYING;
            masterVolume = MAX_VOLUME;
            trackID      = trackBuffer;
            trackBuffer  = -1;
        }
    }

#if RETRO_USING_SDL2 || RETRO_USING_SDL1    
    Sint16 *output_buffer = (Sint16 *)stream;

    size_t samples_remaining = (size_t)len / sizeof(Sint16);
    while (samples_remaining != 0) {
        Sint32 mix_buffer[MIX_BUFFER_SAMPLES];
        memset(mix_buffer, 0, sizeof(mix_buffer));

        const size_t samples_to_do = (samples_remaining < MIX_BUFFER_SAMPLES) ? samples_remaining : MIX_BUFFER_SAMPLES;

        // Mix music
        ProcessMusicStream(mix_buffer, samples_to_do * sizeof(Sint16));

        // Process music being played by a video
        if (videoPlaying) {
            // Fetch THEORAPLAY audio packets, and shove them into the SDL Audio Stream
            const size_t bytes_to_do = samples_to_do * sizeof(Sint16);

            const THEORAPLAY_AudioPacket *packet;

            while ((packet = THEORAPLAY_getAudio(videoDecoder)) != NULL) {
                SDL_AudioStreamPut(ogv_stream, packet->samples, packet->frames * sizeof(float) * 2); // 2 for stereo
                THEORAPLAY_freeAudio(packet);
            }

            Sint16 buffer[MIX_BUFFER_SAMPLES];

            // If we need more samples, assume we've reached the end of the file,
            // and flush the audio stream so we can get more. If we were wrong, and
            // there's still more file left, then there will be a gap in the audio. Sorry.
            if (SDL_AudioStreamAvailable(ogv_stream) < bytes_to_do)
                SDL_AudioStreamFlush(ogv_stream);

            // Fetch the converted audio data, which is ready for mixing.
            int get = SDL_AudioStreamGet(ogv_stream, buffer, bytes_to_do);

            // Mix the converted audio data into the final output
            if (get != -1)
                ProcessAudioMixing(mix_buffer, buffer, get / sizeof(Sint16), MAX_VOLUME, 0);
        }
        else {
            SDL_AudioStreamClear(ogv_stream); // Prevent leftover audio from playing at the start of the next video
        }

        // Mix SFX
        for (byte i = 0; i < CHANNEL_COUNT; ++i) {
            ChannelInfo *sfx = &sfxChannels[i];
            if (sfx == NULL)
                continue;

            if (sfx->sfxID < 0)
                continue;

            if (sfx->samplePtr) {
                Sint16 buffer[MIX_BUFFER_SAMPLES];

                size_t samples_done = 0;
                while (samples_done != samples_to_do) {
                    size_t sampleLen = (sfx->sampleLength < samples_to_do - samples_done) ? sfx->sampleLength : samples_to_do - samples_done;
                    memcpy(&buffer[samples_done], sfx->samplePtr, sampleLen * sizeof(Sint16));

                    samples_done += sampleLen;
                    sfx->samplePtr += sampleLen;
                    sfx->sampleLength -= sampleLen;

                    if (sfx->sampleLength == 0) {
                        if (sfx->loopSFX) {
                            sfx->samplePtr    = sfxList[sfx->sfxID].buffer;
                            sfx->sampleLength = sfxList[sfx->sfxID].length;
                        }
                        else {
                            StopSfx(sfx->sfxID);
                            break;
                        }
                    }
                }

#if RETRO_USING_SDL2
                ProcessAudioMixing(mix_buffer, buffer, samples_done, sfxVolume, sfx->pan);
#endif
            }
        }

        // Clamp mixed samples back to 16-bit and write them to the output buffer
        for (size_t i = 0; i < sizeof(mix_buffer) / sizeof(*mix_buffer); ++i) {
            const Sint16 max_audioval = ((1 << (16 - 1)) - 1);
            const Sint16 min_audioval = -(1 << (16 - 1));

            const Sint32 sample = mix_buffer[i];

            if (sample > max_audioval)
                *output_buffer++ = max_audioval;
            else if (sample < min_audioval)
                *output_buffer++ = min_audioval;
            else
                *output_buffer++ = sample;
        }

        samples_remaining -= samples_to_do;
    }
#else
// Generic (platform-independent) code that mixes music and SFX together into the stream
    int numbytes = AUDIO_SAMPLES * 2 * 2;
    Sint16* streamS = (Sint16*)stream;

    memset(stream, 0x0, numbytes);
    if (musicStatus == MUSIC_READY || musicStatus == MUSIC_PLAYING) {
        char* p = (char*)stream;

        int bytesRead = 0;

	    
	    
        while (bytesRead < numbytes) {
            int r = ov_read(&musInfo.vorbisFile, (char*)p, 256, 0, 2, 1, &musInfo.vorbBitstream);

            if (r == 0) {
                // We've reached the end of the file
                if (musInfo.trackLoop) {
                    ov_pcm_seek(&musInfo.vorbisFile, musInfo.loopPoint);
                    continue;
                } else {
                    musicStatus = MUSIC_STOPPED;
                    break;
                }
            }

            bytesRead += r;
            p += r;
        }

        if (bytesRead > 0) {		
            for (int x = 0; x < bytesRead / 2; x++) {
                Sint32 c = ADJUST_VOLUME(streamS[x], (bgmVolume * masterVolume) / MAX_VOLUME);

                streamS[x] = (c > 0x7FFF) ? 0x7FFF : c;
            }
        }
    }

    for (byte i = 0; i < CHANNEL_COUNT; ++i) {
        ChannelInfo* sfx = &sfxChannels[i];
        if (sfx == NULL)
            continue;

        if (sfx->sfxID < 0)
            continue;

        if (sfx->samplePtr) {
            Sint16 buffer[AUDIO_SAMPLES * 2];

            memset(buffer, 0, numbytes);

            size_t samples_done = 0;
            int samples_to_do = AUDIO_SAMPLES * 2;

            while (samples_done != samples_to_do) {
                size_t sampleLen = (sfx->sampleLength < samples_to_do - samples_done) ? sfx->sampleLength : samples_to_do - samples_done;
                memcpy(&buffer[samples_done], sfx->samplePtr, sampleLen * sizeof(Sint16));

                samples_done += sampleLen;
                sfx->samplePtr += sampleLen;
                sfx->sampleLength -= sampleLen;

                if (sfx->sampleLength <= 0) {
                    if (sfx->loopSFX) {
                        sfx->samplePtr = sfxList[sfx->sfxID].buffer;
                        sfx->sampleLength = sfxList[sfx->sfxID].length;
                    } else {
                        StopSfx(sfx->sfxID);
                        break;
                    }
                }
            }
	    
            for (int x = 0; x < samples_done; x++) {
		    
                Sint32 c = buffer[x] / 2 + streamS[x];
                c = ADJUST_VOLUME(c, (sfxVolume * 100) / MAX_VOLUME);

                streamS[x] = (c > 0x7FFF) ? 0x7FFF : c;		    
            }
        }
    }
#endif

#if RETRO_USING_ALLEGRO4
// Allegro needs unsigned samples		
	for (int x = 0; x < AUDIO_SAMPLES * 2; x++)
		streamS[x] ^= 0x8000;
#endif
	
#endif
}

#if RETRO_USING_SDL2
void ProcessAudioMixing(Sint32 *dst, const Sint16 *src, int len, int volume, sbyte pan)
{
#if !RETRO_DISABLE_AUDIO
    if (volume == 0)
        return;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;

    float panL = 0;
    float panR = 0;
    int i      = 0;

    if (pan < 0) {
        panR = 1.0f - abs(pan / 100.0f);
        panL = 1.0f;
    }
    else if (pan > 0) {
        panL = 1.0f - abs(pan / 100.0f);
        panR = 1.0f;
    }

    while (len--) {
        Sint32 sample = *src++;
        ADJUST_VOLUME(sample, volume);

        if (pan != 0) {
            if ((i % 2) != 0) {
                sample *= panR;
            }
            else {
                sample *= panL;
            }
        }

        *dst++ += sample;

        i++;
    }
}
#endif
#endif

void SetMusicTrack(char *filePath, byte trackID, bool loop, uint loopPoint)
{
#if !RETRO_DISABLE_AUDIO
    LOCK_AUDIO_DEVICE()
    TrackInfo *track = &musicTracks[trackID];
    StrCopy(track->fileName, "Data/Music/");
    StrAdd(track->fileName, filePath);
    track->trackLoop = loop;
    track->loopPoint = loopPoint;
    UNLOCK_AUDIO_DEVICE()
#endif
}
bool PlayMusic(int track)
{
#if !RETRO_DISABLE_AUDIO
    if (!audioEnabled)
        return false;

    LOCK_AUDIO_DEVICE()
    if (track < 0 || track >= TRACK_COUNT) {
        StopMusic();
        trackBuffer = -1;
        return false;
    }
    trackBuffer = track;
    musicStatus = MUSIC_LOADING;
    UNLOCK_AUDIO_DEVICE()
    return true;
#endif
}

#if !RETRO_USING_SDL2 && !RETRO_USING_SDL1
static Sint16* WavDataToBuffer(void* data, int num_frames, int num_channels,
    int bit_depth) 
{
    int outSz = num_frames * 2 * 2;
    Sint16* src = (Sint16*)data;
    unsigned char* src8 = (unsigned char*)data;
    Sint16* out = new Sint16[outSz / num_channels];

    if (num_channels == 2 && bit_depth == 16) {
        memcpy(out, src, outSz);
    } else if (num_channels == 1 && bit_depth == 16) {
        for (int x = 0; x < num_frames; x++) {
            out[x * 2] = src[x];
            out[(x * 2) + 1] = src[x];
        }
    } else if (num_channels == 2 && bit_depth == 8) {
        for (int x = 0; x < num_frames; x++)
            out[x] = (src8[x] << 8) ^ 0x8000;
    } else if (num_channels == 1 && bit_depth == 8) {
        for (int x = 0; x < num_frames; x++) {
            out[x * 2] = (src8[x] << 8) ^ 0x8000;
            out[(x * 2) + 1] = (src8[x] << 8) ^ 0x8000;
        }
    }

    return out;
}
#endif

void LoadSfx(char *filePath, byte sfxID)
{
#if !RETRO_DISABLE_AUDIO
    if (!audioEnabled)
        return;

    FileInfo info;
    char fullPath[0x80];

    StrCopy(fullPath, "Data/SoundFX/");
    StrAdd(fullPath, filePath);

    if (LoadFile(fullPath, &info)) {
        byte *sfx = new byte[info.fileSize];
        FileRead(sfx, info.fileSize);
        CloseFile();

#if RETRO_USING_SDL2
        SDL_LockAudio();
        SDL_RWops *src = SDL_RWFromMem(sfx, info.fileSize);
        if (src == NULL) {
            printLog("Unable to open sfx: %s", info.fileName);
        }
        else {
            SDL_AudioSpec wav_spec;
            uint wav_length;
            byte *wav_buffer;
            SDL_AudioSpec *wav = SDL_LoadWAV_RW(src, 0, &wav_spec, &wav_buffer, &wav_length);

            SDL_RWclose(src);
            delete[] sfx;
            if (wav == NULL) {
                printLog("Unable to read sfx: %s", info.fileName);
            }
            else {
                SDL_AudioCVT convert;
                if (SDL_BuildAudioCVT(&convert, wav->format, wav->channels, wav->freq, audioDeviceFormat.format, audioDeviceFormat.channels,
                                      audioDeviceFormat.freq)
                    > 0) {
                    convert.buf = (byte *)malloc(wav_length * convert.len_mult);
                    convert.len = wav_length;
                    memcpy(convert.buf, wav_buffer, wav_length);
                    SDL_ConvertAudio(&convert);

                    StrCopy(sfxList[sfxID].name, filePath);
                    sfxList[sfxID].buffer = (Sint16 *)convert.buf;
                    sfxList[sfxID].length = convert.len_cvt / sizeof(Sint16);
                    sfxList[sfxID].loaded = true;
                    SDL_FreeWAV(wav_buffer);
                }
                else {
                    StrCopy(sfxList[sfxID].name, filePath);
                    sfxList[sfxID].buffer = (Sint16 *)wav_buffer;
                    sfxList[sfxID].length = wav_length / sizeof(Sint16);
                    sfxList[sfxID].loaded = true;
                }

                std::cout << sfxList[sfxID].name << std::endl;
            }
        }
        SDL_UnlockAudio();
#else
// platform-independent WAV loading code, quite dumb
	    int z =	12;
	    int zid, zs=-8;
	    
	    do {
	        z+=zs+8;    
	        zid = sfx[z] | (sfx[z+1] << 8) | (sfx[z+2] << 16) | (sfx[z+3]<<24);		    
	        zs = sfx[z+4] | (sfx[z+5] << 8) | (sfx[z+6] << 16) | (sfx[z+7]<<24);
	    } while(zid != 0x20746d66); // fmt
	    
            int sample_rate = sfx[z+12] | (sfx[z+13] << 8) | (sfx[z+14] << 16) | (sfx[z+15] << 24);
	    int bit_depth = sfx[z+22] | (sfx[z+23] << 8);
            int num_channels = sfx[z+10] | (sfx[z+11] << 8);

	    z += zs + 8;
	    
	    zs = -8;
	    
	    do {
	        z+=zs+8;    
	        zid = sfx[z] | (sfx[z+1] << 8) | (sfx[z+2] << 16) | (sfx[z+3]<<24);		    
	        zs = sfx[z+4] | (sfx[z+5] << 8) | (sfx[z+6] << 16) | (sfx[z+7]<<24);
	    } while(zid != 0x61746164); // data
	    
	    int data_size = sfx[z+4] | (sfx[z+5] << 8) | (sfx[z+6] << 16) | (sfx[z+7] << 24);
	    int num_frames = (data_size / num_channels) / (bit_depth / 8);
	    
            LOCK_AUDIO_DEVICE()
            StrCopy(sfxList[sfxID].name, filePath);
            sfxList[sfxID].buffer = WavDataToBuffer(&sfx[z+8], num_frames, num_channels,
                bit_depth);
            sfxList[sfxID].length = num_frames * 2;
            sfxList[sfxID].loaded = true;
            UNLOCK_AUDIO_DEVICE()
		
            delete[] sfx;
#endif
    }
#endif
}
void PlaySfx(int sfx, bool loop)
{
#if !RETRO_DISABLE_AUDIO	
    LOCK_AUDIO_DEVICE()
    int sfxChannelID = nextChannelPos++;
    for (int c = 0; c < CHANNEL_COUNT; ++c) {
        if (sfxChannels[c].sfxID == sfx) {
            sfxChannelID = c;
            break;
        }
    }

    ChannelInfo *sfxInfo  = &sfxChannels[sfxChannelID];
    sfxInfo->sfxID        = sfx;
    sfxInfo->samplePtr    = sfxList[sfx].buffer;
    sfxInfo->sampleLength = sfxList[sfx].length;
    sfxInfo->loopSFX      = loop;
    sfxInfo->pan          = 0;
    if (nextChannelPos == CHANNEL_COUNT)
        nextChannelPos = 0;
    UNLOCK_AUDIO_DEVICE()
#endif
}
void SetSfxAttributes(int sfx, int loopCount, sbyte pan)
{
#if !RETRO_DISABLE_AUDIO
    LOCK_AUDIO_DEVICE()
    int sfxChannel = -1;
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID == sfx || sfxChannels[i].sfxID == -1) {
            sfxChannel = i;
            break;
        }
    }
    if (sfxChannel == -1)
        return; // wasn't found

    // TODO: is this right? should it play an sfx here? without this rings dont play any sfx so I assume it must be?
    ChannelInfo *sfxInfo  = &sfxChannels[sfxChannel];
    sfxInfo->samplePtr    = sfxList[sfx].buffer;
    sfxInfo->sampleLength = sfxList[sfx].length;
    sfxInfo->loopSFX      = loopCount == -1 ? sfxInfo->loopSFX : loopCount;
    sfxInfo->pan          = pan;
    sfxInfo->sfxID        = sfx;
    UNLOCK_AUDIO_DEVICE()
#endif
}
