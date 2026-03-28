/**
 * Jitter Utility — Anti-Thundering Herd
 * ======================================
 * When hundreds of offline devices reconnect at end-of-shift (e.g. 5PM),
 * they all fire sync simultaneously. This utility staggers the requests
 * with a random delay to prevent DDoS-ing our own API.
 *
 * Default: 0–30 seconds random delay.
 */

/**
 * Returns a Promise that resolves after a random delay between 0 and maxMs.
 * Used to stagger reconnection sync to prevent thundering herd.
 */
export function randomJitter(maxMs: number = 30_000): Promise<void> {
    const delay = Math.floor(Math.random() * maxMs);
    return new Promise(resolve => setTimeout(resolve, delay));
}

/**
 * Returns a random delay in milliseconds (for use with setTimeout).
 */
export function getJitterMs(maxMs: number = 30_000): number {
    return Math.floor(Math.random() * maxMs);
}
