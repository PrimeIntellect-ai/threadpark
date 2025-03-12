#ifndef THREADPARK_H
#define THREADPARK_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#  ifdef THREAD_PARK_DYNAMIC_LINKING
#    ifdef THREAD_PARK_EXPORTS
#      define THREAD_PARK_EXPORT __declspec(dllexport)
#    else
#      define THREAD_PARK_EXPORT __declspec(dllimport)
#    endif
#  else
#    define THREAD_PARK_EXPORT
#  endif
#else
#  define THREAD_PARK_EXPORT __attribute__((visibility("default"))) __attribute__((used))
#endif

/**
 * @brief Opaque structure representing a thread parking handle.
 *
 * This structure is intentionally hidden from the user. It is created
 * and managed by the ThreadPark library and should only be accessed
 * via the functions provided.
 */
typedef struct tpark_handle_t tpark_handle_t;

/**
 * @brief Create a new thread parking handle.
 *
 * Allocates and initializes an opaque handle used for thread parking.
 * A newly created parking handle has an implicit initial state of "unparked."
 *
 * @return Pointer to a newly allocated tpark_handle_t on success,
 *         or NULL on failure.
 */
THREAD_PARK_EXPORT tpark_handle_t *tparkCreateHandle(void);

/**
 * @brief Prepare to park the current thread (first phase).
 *
 * This call sets an internal "park bit" to indicate that the thread
 * is about to block. It does NOT actually block the calling thread.
 *
 * Using @ref tparkBeginPark in conjunction with @ref tparkWait allows you to ensure
 * that if another thread calls @ref tparkWake between begin and wait, the
 * wait will not block.
 *
 * This two-step approach avoids "lost wake-ups" by ensuring that if
 * another thread calls @ref tparkWake while you are deciding to park,
 * you will not miss it.
 *
 * @param handle Pointer to the thread parking handle.
 */
THREAD_PARK_EXPORT void tparkBeginPark(tpark_handle_t *handle);

/**
 * @brief Actually park (block) the calling thread (second phase).
 *
 * This function causes the calling thread to wait until another
 * thread wakes it via @ref tparkWake. The behavior depends on
 * the `unlocked` parameter:
 *
 * - If `unlocked = false`, @ref tparkWait will first set
 *   the "park bit" (as if @ref tparkBeginPark had been called),
 *   then immediately attempt to block.
 *
 * - If `unlocked = true`, it assumes you have already called
 *   @ref tparkBeginPark while holding your own lock, and then
 *   released that lock. In this mode, @ref tparkWait
 *   will immediately attempt to block the calling thread,
 *   but only if the internal "park bit" is still set.
 *
 * @param handle   Pointer to the thread parking handle.
 * @param unlocked A boolean flag controlling whether the "park bit"
 *                 was already set by @ref tparkBeginPark:
 *                   - false => This call sets the bit itself, then blocks.
 *                   - true  => The bit is assumed to be set already; just block if still needed.
 *
 * @warning tparkWait unblocks *fast*. It is your responsibility to ensure that any state you access after waking
 * up is already visible to the waking thread. If you are accessing state that the waking thread modified moments
 * before waking you up, it may not be visible to you yet. Make sure to use the appropriate memory barriers.
 * @warning The futex wake implementation may not use seq-cst writes. Even seq-cst ordering for all your writes
 * may not guarantee that the observed state is up-to-date. It may be sequentially consistent and un-teared, but
 * not guaranteed up-to-date. It is likely you will not be able to avoid some form of looping in the waking thread
 * until all state is visible.
 */
THREAD_PARK_EXPORT void tparkWait(tpark_handle_t *handle, bool unlocked);

/**
 * @brief Conclude or "undo" the parking state (final phase).
 *
 * This function explicitly clears the internal "park bit." If the thread
 * was never actually blocked, or if @ref tparkWait already returned,
 * calling @ref tparkEndPark is a safe way to ensure the handle is
 * back in a non-parked state for future use.
 *
 * Typical usage:
 *  - If you used @ref tparkBeginPark but discovered you no longer need
 *    to block (e.g., another thread produced data in the meantime),
 *    call @ref tparkEndPark to revert the bit to an "unparked" state
 *    before actually calling @ref tparkWait.
 *  - If you used the two-phase approach and your thread woke up, you
 *    can call @ref tparkEndPark once you are done (e.g., after reacquiring
 *    your mutex) to reset the handle for subsequent use.
 *
 * @param handle Pointer to the thread parking handle.
 */
THREAD_PARK_EXPORT void tparkEndPark(tpark_handle_t *handle);

/**
 * @brief Wake a thread parked on the specified handle.
 *
 * Notifies a thread that is blocked in @ref tparkWait using the same
 * handle. If no thread is currently parked or in the process of parking,
 * this call has no effect.
 *
 * @param handle Pointer to the thread parking handle.
 *               Must have been created by @ref tparkCreateHandle.
 */
THREAD_PARK_EXPORT void tparkWake(tpark_handle_t *handle);

/**
 * @brief Check if a thread is currently parked.
 * @param handle Pointer to the thread parking handle.
 * @return true if the thread is currently parked, false otherwise. This shall mean that it is blocked in @ref tparkWait or that @ref tparkBeginPark was called but @ref tparkWait was not yet called.
 * @warning This is a best-effort function and may reflect outdated state in the moments after a state change.
 */
THREAD_PARK_EXPORT bool tparkIsParked(const tpark_handle_t *handle);

/**
 * @brief Destroy an existing thread parking handle.
 *
 * Cleans up resources associated with the handle. Once destroyed,
 * the handle is no longer valid and must not be used.
 *
 * @param handle Pointer to the thread parking handle to destroy.
 *               Must have been created by @ref tparkCreateHandle.
 */
THREAD_PARK_EXPORT void tparkDestroyHandle(const tpark_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* THREADPARK_H */
