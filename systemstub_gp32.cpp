/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */
#include <bsp.h>
#include <s3c2400.h>
#include <irq.h>
#include <tmacros.h>

#include "systemstub.h"
#include "sound.h"
#include "sfxplayer.h"
#include "xlatgp32.h"

#define FRAMEBUFFER1  0x0C700000
#define FRAMEBUFFER2  0x0C713000

struct SystemStub_GP32 : SystemStub {

	enum {
		SOUND_SAMPLE_RATE = 16000
	};

	uint8* _screen[2];
	uint8 _nflip;
	uint16 _screenW, _screenH;

	struct soundBufParams sbuf;

	virtual ~SystemStub_GP32() {}
	virtual void init(const char *title, uint16 w, uint16 h);
	virtual void destroy();
	virtual void setPalette(const Color *colors, int n);
	virtual void copyRect(const uint8 *buf, uint32 pitch);
	virtual void processEvents();
	virtual void sleep(uint32 duration);
	virtual uint32 getTimeStamp();
	virtual void startAudio(AudioCallback callback, void *param);
	virtual void stopAudio();
	virtual uint32 getOutputSampleRate();
	virtual void *addTimer(void *param, uint32 delay);
	virtual void removeTimer(void *timerId);
	virtual void *createMutex();
	virtual void destroyMutex(void *mutex);
	virtual void lockMutex(void *mutex);
	virtual void unlockMutex(void *mutex);

	void prepareGfxMode();
	void cleanupGfxMode();
	void switchGfxMode(bool fullscreen);
};

SystemStub *SystemStub_GP32_create() {
	return new SystemStub_GP32();
}

void *sbmalloc( int size ) {
	// This SHOULD! be done with malloc() and then
	// fixing caches properly.. but I was lazy
	//
	// Make sure that the memory this function returns has no
	// caches or writeback enabled. Otherwise you will get unwanted
	// side effects - at least on GP32.
	memset((void *)0x0C772000, 0, size);
	return (void *)(0x0C772000);
}

void sbfree( void *p ) {
}

static rtems_irq_connect_data mixer_isr_data;
void sbinstallirq( int i, void (*f)(void), struct soundBufParams *p ) {
	mixer_isr_data.name=BSP_INT_DMA2;
	mixer_isr_data.hdl=(rtems_irq_hdl)f;
	BSP_install_rtems_irq_handler(&mixer_isr_data);
	rINTMSK&=~BIT_DMA2;
}

void sbremoveirq( int i ) {
	BSP_remove_rtems_irq_handler(&mixer_isr_data);
}


void SystemStub_GP32::init(const char *title, uint16 w, uint16 h) {
	memset(&pi, 0, sizeof(pi));
	_screenW = w;
	_screenH = h;
	prepareGfxMode();
}

void SystemStub_GP32::destroy() {
	cleanupGfxMode();
}

#define RGB2INT(r,g,b) (((((r>>3))&0x1f)<<11)|((((g>>3))&0x1f)<<6)|((((b>>3))&0x1f)<<1)|1)

void SystemStub_GP32::setPalette(const Color *colors, int n) {
	assert(n <= 256);
	for (int i = 0; i < n; ++i) {
		const Color *c = &colors[i];
		uint8 r = (c->r << 2) | (c->r & 3);
		uint8 g = (c->g << 2) | (c->g & 3);
		uint8 b = (c->b << 2) | (c->b & 3);
		gp32_setPalette(i,RGB2INT(r,g,b));
	}	
}

void SystemStub_GP32::copyRect(const uint8 *buf, uint32 pitch) {
	XlatVGABuffer((unsigned char*)buf,(unsigned char*)_screen[_nflip]);

	while(((((*(volatile unsigned *)0x14a00010)>>17)&3))!=2);
	while(((((*(volatile unsigned *)0x14a00010)>>17)&3))==2);
	gp32_setFramebuffer(_screen[_nflip]);
	_nflip++;
	if(_nflip>1) _nflip=0;
}

void SystemStub_GP32::processEvents() {
	int gp_buttons=gp32_getButtons();
	pi.quit=(gp_buttons&GP32_KEY_SELECT && gp_buttons&GP32_KEY_START )?true:false;
	if(gp_buttons&GP32_KEY_START){while(gp32_getButtons()&GP32_KEY_START);pi.pause=true;}
	if(gp_buttons&GP32_KEY_SELECT)pi.code=true;
	pi.dirMask=(gp_buttons&GP32_KEY_LEFT)?pi.dirMask|PlayerInput::DIR_LEFT: pi.dirMask & ~PlayerInput::DIR_LEFT;
	pi.dirMask=(gp_buttons&GP32_KEY_RIGHT)?pi.dirMask|PlayerInput::DIR_RIGHT: pi.dirMask & ~PlayerInput::DIR_RIGHT;
	pi.dirMask=(gp_buttons&GP32_KEY_UP)?pi.dirMask|PlayerInput::DIR_UP: pi.dirMask & ~PlayerInput::DIR_UP;
	pi.dirMask=(gp_buttons&GP32_KEY_DOWN)?pi.dirMask|PlayerInput::DIR_DOWN: pi.dirMask & ~PlayerInput::DIR_DOWN;
	pi.button=(gp_buttons&GP32_KEY_A)?true:false;
}

void SystemStub_GP32::sleep(uint32 duration) {
	rtems_task_wake_after((rtems_interval)(duration*TICKS_PER_SECOND)/1000);
}

uint32 SystemStub_GP32::getTimeStamp() {
	rtems_interval now;
	rtems_clock_get( RTEMS_CLOCK_GET_TICKS_SINCE_BOOT, &now);
	return (uint32) (now*1000/TICKS_PER_SECOND);
}

void SystemStub_GP32::startAudio(AudioCallback callback, void *param) {
	initSoundBuffer(SOUND_SAMPLE_RATE,get_PCLK(),&sbuf,sbinstallirq,sbremoveirq,sbmalloc,sbfree,callback,param);
	sbuf.start(&sbuf);
}

void SystemStub_GP32::stopAudio() {
}

uint32 SystemStub_GP32::getOutputSampleRate() {
	return SOUND_SAMPLE_RATE;
}


void *SystemStub_GP32::addTimer(void *param, uint32 delay) {
	rtems_id timer;
	rtems_task_create(
		rtems_build_name(' ',' ',' ',' '),
		2,
		RTEMS_MINIMUM_STACK_SIZE,
		RTEMS_PREEMPT,
		RTEMS_LOCAL,
		&timer
	);
	rtems_task_start(
		timer,
		(rtems_task (*)(rtems_task_argument)) SfxPlayer::eventsCallback,
		(rtems_task_argument) param
	);
	/*rtems_timer_create(
		rtems_build_name(' ',' ',' ',' '),
		&timer
	);
	rtems_timer_fire_after(
		timer,
		(rtems_interval)(delay*TICKS_PER_SECOND)/1000,
		(rtems_timer_service_routine_entry) SfxPlayer::eventsCallback,
		param
	);*/
	
	return (void*)timer;
}



void SystemStub_GP32::removeTimer(void *timerId) {
	rtems_task_delete((rtems_id)timerId);
	
}

void *SystemStub_GP32::createMutex() {
	rtems_id mutex;
	rtems_semaphore_create(rtems_build_name(' ',' ',' ',' '), 1, RTEMS_BINARY_SEMAPHORE, RTEMS_PRIORITY, &mutex);

	return (void*)mutex;
//	return (void*)0;
}

void SystemStub_GP32::destroyMutex(void *mutex) {
	rtems_semaphore_delete((rtems_id)mutex);
}

void SystemStub_GP32::lockMutex(void *mutex) {
	rtems_semaphore_obtain((rtems_id)mutex,RTEMS_DEFAULT_OPTIONS,RTEMS_NO_TIMEOUT);
}

void SystemStub_GP32::unlockMutex(void *mutex) {
	rtems_semaphore_release((rtems_id)mutex);
}

void SystemStub_GP32::prepareGfxMode() {
	_screen[0] = (uint8*) FRAMEBUFFER1;
	_screen[1] = (uint8*) FRAMEBUFFER2;
	_nflip=0;
#if defined GP32_BLUP
	gp32_initFramebufferBP((void*)_screen[1],8,60);
#else
	gp32_initFramebufferN((void*)_screen[1],8,60);
#endif
}

void SystemStub_GP32::cleanupGfxMode() {
}
