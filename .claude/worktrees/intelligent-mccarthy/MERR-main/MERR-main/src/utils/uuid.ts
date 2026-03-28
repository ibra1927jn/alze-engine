/**
 * 🔧 V19: Safe UUID generator with polyfill for older WebViews.
 *
 * crypto.randomUUID() is not available in Android System WebView < v92.
 * This utility provides a standards-compliant v4 UUID fallback using
 * crypto.getRandomValues(), which IS available in all WebView versions
 * that support HTTPS (required for PWAs).
 */

export function safeUUID(): string {
    // Use native if available (fast path)
    if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
        return crypto.randomUUID();
    }

    // Fallback: generate v4 UUID from crypto.getRandomValues
    // Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    const bytes = new Uint8Array(16);
    crypto.getRandomValues(bytes);

    // Set version (4) and variant (10xx) bits per RFC 4122
    bytes[6] = (bytes[6] & 0x0f) | 0x40; // version 4
    bytes[8] = (bytes[8] & 0x3f) | 0x80; // variant 1

    const hex = Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('');

    return [
        hex.slice(0, 8),
        hex.slice(8, 12),
        hex.slice(12, 16),
        hex.slice(16, 20),
        hex.slice(20, 32),
    ].join('-');
}
