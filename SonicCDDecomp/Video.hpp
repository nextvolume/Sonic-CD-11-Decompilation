#ifndef VIDEO_H
#define VIDEO_H

#if !RETRO_DISABLE_OGGTHEORA
#include <theoraplay.h>
#endif

extern int currentVideoFrame;
extern int videoFrameCount;
extern int videoWidth;
extern int videoHeight;
extern float videoAR;

#if !RETRO_DISABLE_OGGTHEORA
extern THEORAPLAY_Decoder *videoDecoder;
extern const THEORAPLAY_VideoFrame *videoVidData;
extern const THEORAPLAY_AudioPacket *videoAudioData;
extern THEORAPLAY_Io callbacks;
#endif

extern byte videoData;
extern int videoFilePos;
extern bool videoPlaying;
extern int vidFrameMS;
extern int vidBaseticks;

void PlayVideoFile(char *filepath);
void UpdateVideoFrame();
int ProcessVideo();
void StopVideoPlayback();

void SetupVideoBuffer(int width, int height);
void CloseVideoBuffer();

#endif // !VIDEO_H
