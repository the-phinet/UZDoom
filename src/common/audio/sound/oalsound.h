/*
** oalsound.h
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 2008-2010 Chris Robinson
** Copyright 2010-2016 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
** Code written prior to 2026 is also licensed under:
**
** SPDX-License-Identifier: BSD-3-Clause
**
**---------------------------------------------------------------------------
**
*/

#ifndef OALSOUND_H
#define OALSOUND_H

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <unordered_map>

#include "i_sound.h"
#include "s_soundinternal.h"

#ifndef NO_OPENAL

#ifdef DYN_OPENAL
#define AL_NO_PROTOTYPES
#include "thirdparty/al.h"
#include "thirdparty/alc.h"
#else
#include "al.h"
#include "alc.h"
#endif

#include "thirdparty/alext.h"


class OpenALSoundStream;

class OpenALSoundRenderer final : public SoundRenderer
{
public:
	OpenALSoundRenderer();
	virtual ~OpenALSoundRenderer();

	virtual void SetSfxVolume(float volume) override;
	virtual void SetMusicVolume(float volume) override;
	virtual void UpdateMusicParams() override;
	virtual SoundHandle LoadSound(uint8_t *sfxdata, int length, int def_loop_start, int def_loop_end) override;
	virtual SoundHandle LoadSoundRaw(uint8_t *sfxdata, int length, int frequency, int channels, int bits, int loopstart, int loopend = -1) override;
	virtual void UnloadSound(SoundHandle sfx) override;
	virtual unsigned int GetMSLength(SoundHandle sfx) override;
	virtual unsigned int GetSampleLength(SoundHandle sfx) override;
	virtual float GetOutputRate() override;

	// Streaming sounds.
	SoundStream *CreateStream(SoundStreamCallback callback, int buffbytes, SampleType stype, ChannelConfig chans, int samplerate, void *userdata) override;

	// Starts a sound.
	FISoundChannel *StartSound(SoundHandle sfx, float vol, float pitch, int chanflags, FISoundChannel *reuse_chan, float startTime) override;
	FISoundChannel *StartSound3D(SoundHandle sfx, SoundListener *listener, float vol, FRolloffInfo *rolloff, float distscale, float pitch, int priority, const FVector3 &pos, const FVector3 &vel, int channum, int chanflags, FISoundChannel *reuse_chan, float startTime) override;

	// Changes a channel's volume.
	virtual void ChannelVolume(FISoundChannel *chan, float volume) override;

	// Changes a channel's pitch.
	virtual void ChannelPitch(FISoundChannel *chan, float pitch) override;

	// Stops a sound channel.
	virtual void StopChannel(FISoundChannel *chan) override;

	// Returns position of sound on this channel, in samples.
	virtual unsigned int GetPosition(FISoundChannel *chan) override;

	// Synchronizes following sound startups.
	virtual void Sync(bool sync) override;

	// Pauses or resumes all sound effect channels.
	virtual void SetSfxPaused(bool paused, int slot) override;

	// Pauses or resumes *every* channel, including environmental reverb.
	virtual void SetInactive(SoundRenderer::EInactiveState inactive) override;

	// Updates the volume, separation, and pitch of a sound channel.
	virtual void UpdateSoundParams3D(SoundListener *listener, FISoundChannel *chan, bool areasound, const FVector3 &pos, const FVector3 &vel) override;

	virtual void UpdateListener(SoundListener *) override;
	virtual void UpdateSounds() override;

	virtual void MarkStartTime(FISoundChannel*, float startTime) override;
	virtual float GetAudibility(FISoundChannel*) override;


	virtual bool IsValid() override;
	virtual void PrintStatus() override;
	virtual void PrintDriversList() override;
	virtual FString GatherStats() override;

private:
	struct {
		bool EXT_EFX : 1;
		bool EXT_disconnect : 1;
		bool SOFT_HRTF : 1;
		bool SOFT_pause_device : 1;
		bool SOFT_output_limiter : 1;
	} ALC;
	struct {
		bool EXT_source_distance_model : 1;
		bool EXT_SOURCE_RADIUS : 1;
		bool SOFT_deferred_updates : 1;
		bool SOFT_direct_channels_remix : 1;
		bool SOFT_loop_points : 1;
		bool SOFT_source_latency : 1;
		bool SOFT_source_resampler : 1;
		bool SOFT_source_spatialize : 1;
		bool SOFT_UHJ : 1;
	} AL;

	// EFX Extension function pointer variables. Loaded after context creation
	// if EFX is supported. These pointers may be context- or device-dependant,
	// thus can't be static
	// Effect objects
	LPALGENEFFECTS alGenEffects;
	LPALDELETEEFFECTS alDeleteEffects;
	LPALISEFFECT alIsEffect;
	LPALEFFECTI alEffecti;
	LPALEFFECTIV alEffectiv;
	LPALEFFECTF alEffectf;
	LPALEFFECTFV alEffectfv;
	LPALGETEFFECTI alGetEffecti;
	LPALGETEFFECTIV alGetEffectiv;
	LPALGETEFFECTF alGetEffectf;
	LPALGETEFFECTFV alGetEffectfv;
	// Filter objects
	LPALGENFILTERS alGenFilters;
	LPALDELETEFILTERS alDeleteFilters;
	LPALISFILTER alIsFilter;
	LPALFILTERI alFilteri;
	LPALFILTERIV alFilteriv;
	LPALFILTERF alFilterf;
	LPALFILTERFV alFilterfv;
	LPALGETFILTERI alGetFilteri;
	LPALGETFILTERIV alGetFilteriv;
	LPALGETFILTERF alGetFilterf;
	LPALGETFILTERFV alGetFilterfv;
	// Auxiliary slot objects
	LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
	LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
	LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
	LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
	LPALAUXILIARYEFFECTSLOTIV alAuxiliaryEffectSlotiv;
	LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf;
	LPALAUXILIARYEFFECTSLOTFV alAuxiliaryEffectSlotfv;
	LPALGETAUXILIARYEFFECTSLOTI alGetAuxiliaryEffectSloti;
	LPALGETAUXILIARYEFFECTSLOTIV alGetAuxiliaryEffectSlotiv;
	LPALGETAUXILIARYEFFECTSLOTF alGetAuxiliaryEffectSlotf;
	LPALGETAUXILIARYEFFECTSLOTFV alGetAuxiliaryEffectSlotfv;

	ALvoid (AL_APIENTRY*alDeferUpdatesSOFT)(void);
	ALvoid (AL_APIENTRY*alProcessUpdatesSOFT)(void);

	LPALGETSTRINGISOFT alGetStringiSOFT;

	LPALGETSOURCEI64VSOFT alGetSourcei64vSOFT;

	void (ALC_APIENTRY*alcDevicePauseSOFT)(ALCdevice *device);
	void (ALC_APIENTRY*alcDeviceResumeSOFT)(ALCdevice *device);

	void BackgroundProc();
	void AddStream(OpenALSoundStream *stream);
	void RemoveStream(OpenALSoundStream *stream);

	void LoadReverb(const ReverbContainer *env);
	void PurgeStoppedSources();
	static FSoundChan *FindLowestChannel();

	std::thread StreamThread;
	std::mutex StreamLock;
	std::condition_variable StreamWake;
	std::atomic<bool> QuitThread;

	ALCdevice *Device;
	ALCcontext *Context;

	TArray<ALuint> Sources;

	ALfloat SfxVolume;
	ALfloat MusicVolume;

	int SFXPaused;
	TArray<ALuint> FreeSfx;
	TArray<ALuint> PausableSfx;
	TArray<ALuint> ReverbSfx;
	TArray<ALuint> SfxGroup;

	const ReverbContainer *PrevEnvironment;

	typedef TMap<uint16_t,ALuint> EffectMap;
	typedef TMapIterator<uint16_t,ALuint> EffectMapIter;
	ALuint EnvSlot;
	ALuint EnvFilters[2];
	EffectMap EnvEffects;

	bool WasInWater;

	TArray<OpenALSoundStream*> Streams;
	friend class OpenALSoundStream;

	ALCdevice *InitDevice();
};

#endif // NO_OPENAL

#endif
