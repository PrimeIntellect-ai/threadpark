#ifndef THREADPARK_H
#define THREADPARK_H

#ifdef __cplusplus
extern "C" {
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
 * @def THREAD_PARK_EXPORT
 * @brief Export macro for symbols.
 *
 * Marks symbols as visible and used for shared library usage on
 * compilers that support GCC-style attributes.
 */
#define THREAD_PARK_EXPORT __attribute__((visibility("default"))) __attribute__((used))

/**
 * @brief Create a new thread parking handle.
 *
 * Allocates and initializes an opaque handle used for thread parking.
 *
 * @return Pointer to a newly allocated tpark_handle_t on success,
 *         or NULL on failure.
 */
THREAD_PARK_EXPORT tpark_handle_t *tparkCreateHandle(void);

/**
 * @brief Park (block) the calling thread using the specified handle.
 *
 * This function will cause the calling thread to wait until another
 * thread wakes it via @ref tparkWake.
 *
 * @param handle Pointer to the thread parking handle.
 *               Must have been created by @ref tparkCreateHandle.
 */
THREAD_PARK_EXPORT void tparkPark(tpark_handle_t *handle);

/**
 * @brief Wake a thread parked on the specified handle.
 *
 * Notifies a thread that is blocked in @ref tparkPark using the same
 * handle. If no thread is currently parked, this call has no effect.
 *
 * @param handle Pointer to the thread parking handle.
 *               Must have been created by @ref tparkCreateHandle.
 */
THREAD_PARK_EXPORT void tparkWake(tpark_handle_t *handle);

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
