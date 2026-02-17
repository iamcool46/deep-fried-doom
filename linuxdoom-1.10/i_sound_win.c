//-----------------------------------------------------------------------------
// Windows sound implementation using XAudio2
// Plays DOOM's raw 8-bit 11025Hz unsigned PCM sound lumps
//-----------------------------------------------------------------------------
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_sound.h"
#include "w_wad.h"
#include "sounds.h"
#include "z_zone.h"

#pragma comment(lib, "xaudio2.lib")

// ============================================================
// DOOM sound lump header (8 bytes before the PCM data)
// ============================================================
typedef struct {
    unsigned short format;      // Always 3 (PC Speaker format) or 0
    unsigned short sample_rate; // Usually 11025
    unsigned int   num_samples; // Number of samples
} doom_sound_header_t;

// ============================================================
// Channel state
// ============================================================
#define NUM_CHANNELS 8

typedef struct {
    IXAudio2SourceVoice* voice;
    int                  sfx_id;
    int                  in_use;
    void* buffer_data;  // malloc'd PCM - freed after stop
} sound_channel_t;

// ============================================================
// Globals
// ============================================================
int snd_MusicDevice;
int snd_SfxDevice;
int snd_DesiredMusicDevice;
int snd_DesiredSfxDevice;

static IXAudio2* s_xaudio2 = NULL;
static IXAudio2MasteringVoice* s_master = NULL;
static sound_channel_t        s_channels[NUM_CHANNELS];
static int                    s_sound_ok = 0;

// Cached converted sound data (16-bit signed, 11025 Hz)
static void* s_sfx_cache[NUMSFX];
static int    s_sfx_size[NUMSFX];  // size in BYTES of 16-bit data

// ============================================================
// Convert DOOM 8-bit unsigned PCM -> 16-bit signed PCM
// ============================================================
static short* ConvertDoomSound(const byte* lump_data, int lump_size, int* out_samples)
{
    const doom_sound_header_t* hdr = (const doom_sound_header_t*)lump_data;
    int num_samples;
    const byte* src;
    short* dst;
    int i;

    // Validate
    if (lump_size < 8)
    {
        *out_samples = 0;
        return NULL;
    }

    num_samples = (int)hdr->num_samples;
    if (num_samples <= 0 || num_samples > lump_size - 8)
        num_samples = lump_size - 8;

    if (num_samples <= 0)
    {
        *out_samples = 0;
        return NULL;
    }

    dst = (short*)malloc(num_samples * sizeof(short));
    if (!dst)
    {
        *out_samples = 0;
        return NULL;
    }

    src = lump_data + 8;  // Skip header
    for (i = 0; i < num_samples; i++)
    {
        // Convert unsigned 8-bit [0..255] to signed 16-bit [-32768..32767]
        dst[i] = ((int)src[i] - 128) * 256;
    }

    *out_samples = num_samples;
    return dst;
}

// ============================================================
// Pre-cache a sound effect
// ============================================================
static void CacheSfx(int sfx_id)
{
    char name[16];
    int lump;
    byte* lump_data;
    int lump_size;
    int num_samples;
    short* pcm;

    if (sfx_id < 1 || sfx_id >= NUMSFX)
        return;
    if (s_sfx_cache[sfx_id])
        return;  // already cached

    sprintf(name, "ds%s", S_sfx[sfx_id].name);
    lump = W_CheckNumForName(name);
    if (lump < 0)
        return;

    lump_size = W_LumpLength(lump);
    lump_data = (byte*)W_CacheLumpNum(lump, PU_STATIC);
    if (!lump_data)
        return;

    pcm = ConvertDoomSound(lump_data, lump_size, &num_samples);
    if (!pcm || num_samples <= 0)
        return;

    s_sfx_cache[sfx_id] = pcm;
    s_sfx_size[sfx_id] = num_samples * sizeof(short);
}

// ============================================================
// I_InitSound
// ============================================================
void I_InitSound(void)
{
    HRESULT hr;
    int i;

    s_sound_ok = 0;
    memset(s_channels, 0, sizeof(s_channels));
    memset(s_sfx_cache, 0, sizeof(s_sfx_cache));
    memset(s_sfx_size, 0, sizeof(s_sfx_size));

    // Initialize COM
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // Create XAudio2 engine
    hr = XAudio2Create(&s_xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        fprintf(stderr, "I_InitSound: XAudio2Create failed (0x%08X)\n", hr);
        return;
    }

    // Create mastering voice (output to default audio device)
    hr = s_xaudio2->lpVtbl->CreateMasteringVoice(
        s_xaudio2, &s_master,
        XAUDIO2_DEFAULT_CHANNELS,
        XAUDIO2_DEFAULT_SAMPLERATE,
        0, NULL, NULL, AudioCategory_GameEffects);

    if (FAILED(hr))
    {
        fprintf(stderr, "I_InitSound: CreateMasteringVoice failed (0x%08X)\n", hr);
        s_xaudio2->lpVtbl->Release(s_xaudio2);
        s_xaudio2 = NULL;
        return;
    }

    s_sound_ok = 1;
    fprintf(stderr, "I_InitSound: XAudio2 initialized OK\n");

    // Pre-cache all sounds
    fprintf(stderr, "I_InitSound: Pre-caching sounds...\n");
    for (i = 1; i < NUMSFX; i++)
        CacheSfx(i);
    fprintf(stderr, "I_InitSound: Sound cache ready\n");
}

// ============================================================
// I_ShutdownSound
// ============================================================
void I_ShutdownSound(void)
{
    int i;
    if (!s_sound_ok) return;

    for (i = 0; i < NUM_CHANNELS; i++)
    {
        if (s_channels[i].voice)
        {
            s_channels[i].voice->lpVtbl->DestroyVoice(s_channels[i].voice);
            s_channels[i].voice = NULL;
        }
    }

    if (s_master)
    {
        s_master->lpVtbl->DestroyVoice(s_master);
        s_master = NULL;
    }

    if (s_xaudio2)
    {
        s_xaudio2->lpVtbl->Release(s_xaudio2);
        s_xaudio2 = NULL;
    }

    // Free cached sounds
    for (i = 1; i < NUMSFX; i++)
    {
        if (s_sfx_cache[i])
        {
            free(s_sfx_cache[i]);
            s_sfx_cache[i] = NULL;
        }
    }

    s_sound_ok = 0;
    CoUninitialize();
}

// ============================================================
// I_UpdateSound / I_SubmitSound / I_SetChannels
// ============================================================
void I_UpdateSound(void) {}
void I_SubmitSound(void) {}
void I_SetChannels(void) {}

// ============================================================
// I_GetSfxLumpNum
// ============================================================
int I_GetSfxLumpNum(sfxinfo_t* sfxinfo)
{
    char name[20];
    int lump;
    if (!sfxinfo || !sfxinfo->name) return 0;
    sprintf(name, "ds%s", sfxinfo->name);
    lump = W_CheckNumForName(name);
    if (lump < 0)
        lump = W_CheckNumForName("dspistol");
    return (lump < 0) ? 0 : lump;
}

// ============================================================
// I_StartSound - returns channel handle
// ============================================================
int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    WAVEFORMATEX wfx;
    XAUDIO2_BUFFER buf;
    IXAudio2SourceVoice* voice;
    sound_channel_t* ch;
    HRESULT hr;
    float pan_left, pan_right;
    float volumes[2];
    int i;
    int best = -1;

    (void)pitch;
    (void)priority;

    if (!s_sound_ok) return -1;
    if (id < 1 || id >= NUMSFX) return -1;

    // Make sure sound is cached
    CacheSfx(id);
    if (!s_sfx_cache[id]) return -1;

    // Find a free channel (or steal the oldest)
    for (i = 0; i < NUM_CHANNELS; i++)
    {
        if (!s_channels[i].in_use) { best = i; break; }
    }
    if (best < 0) best = 0;  // Steal channel 0

    ch = &s_channels[best];

    // Stop existing voice on this channel
    if (ch->voice)
    {
        ch->voice->lpVtbl->Stop(ch->voice, 0, XAUDIO2_COMMIT_NOW);
        ch->voice->lpVtbl->DestroyVoice(ch->voice);
        ch->voice = NULL;
    }
    ch->in_use = 0;

    // Build wave format: 16-bit signed PCM, mono, 11025 Hz
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 11025;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = 11025 * 2;

    hr = s_xaudio2->lpVtbl->CreateSourceVoice(
        s_xaudio2, &voice, &wfx,
        0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);

    if (FAILED(hr)) return -1;

    // Submit the audio buffer
    memset(&buf, 0, sizeof(buf));
    buf.AudioBytes = (UINT32)s_sfx_size[id];
    buf.pAudioData = (BYTE*)s_sfx_cache[id];
    buf.Flags = XAUDIO2_END_OF_STREAM;

    hr = voice->lpVtbl->SubmitSourceBuffer(voice, &buf, NULL);
    if (FAILED(hr))
    {
        voice->lpVtbl->DestroyVoice(voice);
        return -1;
    }

    // Set volume (0..127 -> 0.0..1.0)
    voice->lpVtbl->SetVolume(voice, (float)vol / 127.0f, XAUDIO2_COMMIT_NOW);

    // Set stereo pan from sep (0=left, 128=center, 255=right)
    pan_right = (float)sep / 255.0f;
    pan_left = 1.0f - pan_right;
    volumes[0] = pan_left;
    volumes[1] = pan_right;
    voice->lpVtbl->SetOutputMatrix(voice, NULL, 1, 2, volumes, XAUDIO2_COMMIT_NOW);

    // Play it
    voice->lpVtbl->Start(voice, 0, XAUDIO2_COMMIT_NOW);

    ch->voice = voice;
    ch->sfx_id = id;
    ch->in_use = 1;

    return best;
}

// ============================================================
// I_StopSound
// ============================================================
void I_StopSound(int handle)
{
    if (handle < 0 || handle >= NUM_CHANNELS) return;
    if (!s_channels[handle].voice) return;

    s_channels[handle].voice->lpVtbl->Stop(s_channels[handle].voice, 0, XAUDIO2_COMMIT_NOW);
    s_channels[handle].voice->lpVtbl->DestroyVoice(s_channels[handle].voice);
    s_channels[handle].voice = NULL;
    s_channels[handle].in_use = 0;
}

// ============================================================
// I_SoundIsPlaying
// ============================================================
int I_SoundIsPlaying(int handle)
{
    XAUDIO2_VOICE_STATE state;
    if (handle < 0 || handle >= NUM_CHANNELS) return 0;
    if (!s_channels[handle].voice) return 0;

    s_channels[handle].voice->lpVtbl->GetState(
        s_channels[handle].voice, &state, 0);

    if (state.BuffersQueued == 0)
    {
        // Sound finished - clean up
        s_channels[handle].voice->lpVtbl->DestroyVoice(s_channels[handle].voice);
        s_channels[handle].voice = NULL;
        s_channels[handle].in_use = 0;
        return 0;
    }
    return 1;
}

// ============================================================
// I_UpdateSoundParams
// ============================================================
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    float pan_left, pan_right, volumes[2];
    (void)pitch;

    if (handle < 0 || handle >= NUM_CHANNELS) return;
    if (!s_channels[handle].voice) return;

    s_channels[handle].voice->lpVtbl->SetVolume(
        s_channels[handle].voice, (float)vol / 127.0f, XAUDIO2_COMMIT_NOW);

    pan_right = (float)sep / 255.0f;
    pan_left = 1.0f - pan_right;
    volumes[0] = pan_left;
    volumes[1] = pan_right;
    s_channels[handle].voice->lpVtbl->SetOutputMatrix(
        s_channels[handle].voice, NULL, 1, 2, volumes, XAUDIO2_COMMIT_NOW);
}

// ============================================================
// Music stubs (can add MIDI later via winmm)
// ============================================================
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_SetMusicVolume(int volume) { (void)volume; }
void I_PauseSong(int handle) { (void)handle; }
void I_ResumeSong(int handle) { (void)handle; }
int  I_RegisterSong(void* data) { (void)data; return 0; }
void I_PlaySong(int handle, int looping) { (void)handle; (void)looping; }
void I_StopSong(int handle) { (void)handle; }
void I_UnRegisterSong(int handle) { (void)handle; }

#endif /* _WIN32 */