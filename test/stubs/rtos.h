/* Host-test stub for mbed rtos.h - just enough for Scroller.h.
 * The real firmware never sees this file (explicit object list in the
 * Makefile); it exists only for the host unit tests in test/.
 */
#ifndef HOST_STUB_RTOS_H_
#define HOST_STUB_RTOS_H_

class Semaphore {
public:
	Semaphore(int) {}
	void wait() {}
	void release() {}
};

#endif
