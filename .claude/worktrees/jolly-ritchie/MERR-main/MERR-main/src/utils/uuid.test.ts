import { describe, it, expect, vi } from 'vitest';
import { safeUUID } from './uuid';

describe('safeUUID — UUID v4 generator with polyfill', () => {
    it('returns a valid UUID v4 format', () => {
        const uuid = safeUUID();
        expect(uuid).toMatch(
            /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/
        );
    });

    it('generates unique UUIDs', () => {
        const uuids = new Set(Array.from({ length: 100 }, () => safeUUID()));
        expect(uuids.size).toBe(100);
    });

    it('uses native crypto.randomUUID when available', () => {
        const mockUUID = '12345678-1234-4123-a123-123456789abc';
        const spy = vi.spyOn(crypto, 'randomUUID').mockReturnValue(mockUUID as `${string}-${string}-${string}-${string}-${string}`);

        const result = safeUUID();
        expect(result).toBe(mockUUID);
        expect(spy).toHaveBeenCalledOnce();

        spy.mockRestore();
    });

    it('falls back to getRandomValues when randomUUID is unavailable', () => {
        const original = crypto.randomUUID;
        // @ts-expect-error — testing the fallback path
        crypto.randomUUID = undefined;

        const uuid = safeUUID();
        expect(uuid).toMatch(
            /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/
        );

        crypto.randomUUID = original;
    });

    it('version bit is always 4', () => {
        for (let i = 0; i < 50; i++) {
            const uuid = safeUUID();
            expect(uuid[14]).toBe('4');
        }
    });

    it('variant bits are 10xx (8, 9, a, or b)', () => {
        for (let i = 0; i < 50; i++) {
            const uuid = safeUUID();
            expect(['8', '9', 'a', 'b']).toContain(uuid[19]);
        }
    });
});
