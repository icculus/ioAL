/**
 * An OpenAL implementation.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef _INCL_ALCORE_H_
#define _INCL_ALCORE_H_

/*
 * This is the core of the AL...this handles all the general state changes and
 *  dispatching to a device implementation for rendering.
 */

/*
 * WHERE TO LOOK IN HERE:
 *
 *  - If you want to add a device implementation, please study the
 *    __alDeviceInterface structure in this file. You will be implementing the
 *    methods in that structure and adding a structure full of pointers to your
 *    implementation to a table. Device implementations are generally higher
 *    level: you get details about AL state, and provide the rendering of that
 *    state yourself...this is for gluing to other high-level 3D APIs or a
 *    true hardware implementation. Most people do not need to work in this
 *    interface.
 *  - If you want to add an output target (i.e. - glue an API that that wants
 *    rendered audio fed to it for playback), you should look at alMixer.h;
 *    this is the included software mixer that will render a PCM stream and
 *    feed it to another API or a basic DSP.
 *  - If you want to add an extension, you may need to touch several pieces
 *    of the AL. You will want to start with alPublic.c, which implements
 *    the application entry points.
 */


/*
 * Some wrappers to make opaque data typesafe...
 */
typedef struct S_ALSRCIMPL
{
    void *opaque;
} __alSourceImpl;

typedef struct S_ALBUFIMPL
{
    void *opaque;
} __alBufferImpl;

typedef struct S_ALCTXIMPL
{
    void *opaque;
} __alContextImpl;

typedef struct S_ALDEVIMPL
{
    void *opaque;
} __alDeviceImpl;


/*
 * Source state.
 */
typedef struct S_ALSRC
{
    /* !!! FIXME: Fill in state here. */
    __alSourceImpl *impl;
} __alSource;


/*
 * Buffer state.
 */
typedef struct S_ALBUF
{
    /* !!! FIXME: Fill in state here. */
    __alBufferImpl *impl;
} __alBuffer;


/*
 * Context state.
 */
typedef struct S_ALCTX
{
    /* !!! FIXME: Fill in state here. */
    __alContextImpl *impl;
} __alContext;





/*
 * The device interface; this is what various hardware implementations should
 *  fill in to hook into the library. The software mixer also uses this
 *  interface on behalf of various output targets...if you are just gluing an
 *  API to OpenAL (such as DirectSound or ALSA or SDL), then you shouldn't
 *  implement this interface, and instead let the software mixer do the
 *  work...there is a different abstract interface that the software mixer
 *  uses for targets that just need to be fed a pre-mixed audio stream.
 *
 * You need to fill in an __alDeviceInterface structure and make sure it's in
 *  the __alDeviceInterfaces table in alCore.c. alcOpenDevice() will iterate
 *  through this table and ask each device if they can claim the device name.
 *  If so, then other entry points in the interface will be called as needed.
 *
 * The comments below discuss the software mixer implementation, but a
 *  hardware-accelerated implementation should be able to accomplish much
 *  of the same.
 *
 * Please note that this library is multithreaded and intended to be
 *  thread safe to the calling application, as well. However, much of the
 *  synchronization is handled above the device interface. Exceptions are
 *  documented, below.
 */
typedef struct S_ALDEVINTERFACE
{
    /*
     * Enumerate device names. This function is used by ALC_ENUMERATION_EXT.
     *  Call the supplied callback with all reasonable device names you'll
     *  accept. Since some devices aren't reasonable to enumerate, such as a
     *  remote audio daemon, this is not used by alcOpenDevice(), so you may
     *  still get requests to open device names that aren't in your
     *  enumeration. In the same vein, if you can't enumerate every device,
     *  or any, just report what's reasonable and return.
     *
     * This method may be called at any time, with or without an opened
     *  device.
     */
    void (*enumerate)(void (*callback)(const ALubyte *name));

    /*
     * "Open" the device. This is meant to give the implementor a chance to
     *   acquire resources, but the hardware is not at this point configured
     *   with any sort of output format (see "configure" method).
     *
     * The implementation is to examine the device name and decide if this
     *  is a device they own, and return zero immediately if not. The AL will
     *  iterate over all device interfaces until one claims the device name.
     *
     * Device interfaces should not be considered singleton in nature; if you
     *  can handle multiple openings of your device (or multiple devices at
     *  the same time) you should. Failing to open more than once is not
     *  unheard of, though.
     *
     * If you can claim this device name, allocate your instance data and
     *  return a pointer to it. Otherwise, return NULL.
     */
    __alDeviceImpl *(*open)(const ALubyte *devname);

    /*
     * Configure the device for output. While the "open" method is analogous
     *  to alcOpenDevice(), "configure" is associated with alcCreateContext(),
     *  and as such may be called multiple times on an open device by the AL.
     * Configuring twice with different frequencies can be considered a
     *  legitimate error, and the device implementor can opt to fail here.
     * Also, most attributes may be ignored or taken as a hint.
     *
     * !!! ALFIXME: Specifying device details during context creation is
     * !!! ALFIXME:  stupid. We should fix this in the AL spec.
     */
    int (*configure)(__alDeviceImpl *dev, ALint *attributes);

    /*
     * Stop all playback, close the device and release any resources used by
     *  it. The device should be prepared for future calls to the open()
     *  method.
     *
     * Sources and buffers aren't guaranteed to be cleaned up before this
     *  call; please invalidate and free them if necessary.
     *
     * Unless otherwise noted, all members of this interface are only
     *  valid between a successful open() and a matching close() call.
     */
    void (*close)(__alDeviceImpl *dev);

    /*
     * Allocate a context...The AL calls this from the alcCreateContext()
     *  entry point. You are free to implement multiple contexts, but most
     *  software doesn't use more than one...this should not be seen as an
     *  excuse to be lazy, though.
     *
     * If you can allocate another context on the device, return a pointer
     *  to instance data for this context. Otherwise, return NULL.
     */
    __alContextImpl *(*allocateContext)(__alDeviceImpl *dev);

    /*
     * Free a previously allocated context. This is called from
     *  alcDestroyContext. This is guaranteed not to be the
     *  current context.
     */
    void (*freeContext)(__alDeviceImpl *dev, __alContextImpl *ctx);

    /*
     * Allocate a source...The AL calls this from the alGenSources() entry
     *  point. Sources are intended to be a finite resource and
     *  the device implementation shouldn't fall back to a slow path if the
     *  resource is depleted.
     *
     * The software mixer has an arbitrary limit to guarantee that repeated
     *  calls to this function will eventually fail...some applications will
     *  continue to call alGenSources() in a loop until it fails, so this is
     *  a requirement.
     *
     * If you can allocate another source on the device, return a pointer
     *  to instance data for this source. Otherwise, return NULL.
     */
    __alSourceImpl *(*allocateSource)(__alDeviceImpl *dev, __alContextImpl *ctx);

    /*
     * Free a previously allocated source. This is called from alDeleteSources.
     *  This may allow a hardware source to be reallocated to another context
     *  on the same device in a future alGenSources() call.
     */
    void (*freeSource)(__alDeviceImpl *dev, __alSourceImpl *src);

    /*
     * Allocate a buffer...The AL calls this from the alGenBuffer() entry
     *  point. Unlike sources, you should strive to generate as many buffers
     *  as possible, and implement some intelligence to shuffle buffer data
     *  between system RAM and the hardware as they are assigned to sources.
     *  Most applications consider buffers to be a more-or-less infinite
     *  resource. At least, buffer names...it might be better to fail in
     *  uploadBuffer(), where significant resources are at stake.
     *
     *  !!! ALFIXME: A way to prioritize buffers on the hardware would be nice.
     *  !!! ALFIXME:  something like glPrioritizeTexture
     */
    __alBufferImpl *(*allocateBuffer)(__alDeviceImpl *dev);

    /*
     * Free a previously allocated buffer. This is called from alDeleteBuffers.
     *  You should free the buffer name and and buffered data.
     */
    void (*freeBuffer)(__alDeviceImpl *dev, __alBufferImpl *buf);

    /*
     * Prepare a buffer for playback. The AL calls this from alBufferData().
     *  You can consider this a "slow" call and take the effort to convert
     *  and resample the data as needed...you must copy here, though, since
     *  the original data is considered read-only and may be thrown away or
     *  freed by the application after this call. Please note that in
     *  resource-constrained hardware implementations, it might be
     *  advantageous to defer the real upload until the buffer is actually
     *  assigned to a source via AL_BUFFER or the buffer queueing mechanism.
     *
     * Returns an error code (AL_OUT_OF_MEMORY, etc) on failure, or
     *  AL_NO_ERROR on success.
     */
    ALenum (*uploadBuffer)(__alDeviceImpl *dev, __alBufferImpl *buf,
                           ALenum fmt, ALvoid *data, ALsizei freq);

    /*
     * This is called when preparing to process a context and a source's
     *  state has changed since the last time the context was processed.
     *  You should update the device to reflect the new state.
     *  The associated __alSourceImpl pointer is in the __alSource structure.
     *
     * The AL is multithreaded, and expects to block the main application for
     *  as little time as possible. As such, all state changes are deferred
     *  until the context is processed: either manually, or with a regular
     *  frequency for an asynchronous context. At that time, a lock is
     *  held while deferred state is committed, but the lock is not held
     *  during rendering, so you may not keep these structures around, as
     *  they are likely to change or disappear between commits. If you need
     *  to store state information outside of the device, you will need to
     *  copy this structure.
     */
    void (*commitSource)(__alDeviceImpl *dev, const __alSource *src);

    /*
     * This is called when preparing to process a context and a buffer's
     *  state has changed since the last time the context was processed.
     *  You should update the device to reflect the new state.
     *  The associated __alBufferImpl pointer is in the __alBuffer structure.
     *
     * Please see comments about multithreading in commitSource(), above.
     */
    void (*commitBuffer)(__alDeviceImpl *dev, const __alBuffer *buf);

    /*
     * This is called when preparing to process a context and some of its state
     *  that isn't encapsulated in buffer or sources (such as the Listener
     *  and reference distance, etc) has changed since the last time the
     *  context was processed. You should update the device to reflect the new
     *  state. The associated __alContextImpl pointer is in the __alContext
     *  structure.
     *
     * Please see comments about multithreading in commitSource(), above.
     */
    void (*commitContext)(__alDeviceImpl *dev, const __alContext *ctx);

    /*
     * Do rendering, etc. If your implementation is running in parallel, this
     *  might be a no-op. You can use this for general device upkeep, since
     *  it will be called regularly.
     */
    void (*upkeep)(__alDeviceImpl *dev);
} __alDeviceInterface;

extern __alDeviceInterface **__alDeviceInterfaces;


/*
 * alcOpenDevice() returns an opaque pointer to an __alDevice; this is what
 *  we use as the center of all activity in the AL core.
 */
typedef struct S_ALDEV
{
    __alDeviceInterface *interface;
    __alDeviceImpl *impl;
    __alContext *contexts;
    ALuint bufferCount;
    __alBuffer *buffers;  /* buffers are shared between ctxs on the device */
} __alDevice;

#endif

/* end of alCore.h ... */

