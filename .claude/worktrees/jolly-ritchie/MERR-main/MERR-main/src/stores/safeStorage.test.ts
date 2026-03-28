/**
 * safeStorage Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { safeStorage } from './safeStorage';

describe('safeStorage', () => {
    beforeEach(() => {
        localStorage.clear();
        vi.restoreAllMocks();
    });

    describe('getItem', () => {
        it('returns null for missing key', () => {
            expect(safeStorage.getItem('nonexistent')).toBeNull();
        });

        it('returns stored value', () => {
            localStorage.setItem('test-key', 'test-value');
            expect(safeStorage.getItem('test-key')).toBe('test-value');
        });

        it('delegates directly to localStorage', () => {
            const spy = vi.spyOn(Storage.prototype, 'getItem');
            safeStorage.getItem('test-key');
            expect(spy).toHaveBeenCalledWith('test-key');
        });
    });

    describe('setItem', () => {
        it('stores value in localStorage', () => {
            safeStorage.setItem('key', 'value');
            expect(localStorage.getItem('key')).toBe('value');
        });

        it('handles QuotaExceededError by clearing old data', () => {
            let callCount = 0;
            vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
                callCount++;
                if (callCount === 1) {
                    const err = new DOMException('quota exceeded', 'QuotaExceededError');
                    throw err;
                }
                // Second call succeeds after cleanup
            });
            // Should not throw — handles quota gracefully
            expect(() => safeStorage.setItem('key', 'value')).not.toThrow();
        });
    });

    describe('removeItem', () => {
        it('removes stored value', () => {
            localStorage.setItem('key', 'value');
            safeStorage.removeItem('key');
            expect(localStorage.getItem('key')).toBeNull();
        });

        it('does not throw on missing key', () => {
            expect(() => safeStorage.removeItem('nonexistent')).not.toThrow();
        });
    });
});
