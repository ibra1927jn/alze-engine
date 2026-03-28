import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { randomJitter, getJitterMs } from './jitter';

describe('jitter — Anti-Thundering Herd utility', () => {
    beforeEach(() => {
        vi.useFakeTimers();
    });

    afterEach(() => {
        vi.useRealTimers();
    });

    describe('getJitterMs', () => {
        it('returns a number between 0 and maxMs', () => {
            for (let i = 0; i < 100; i++) {
                const delay = getJitterMs(5000);
                expect(delay).toBeGreaterThanOrEqual(0);
                expect(delay).toBeLessThan(5000);
            }
        });

        it('defaults to max 30000ms', () => {
            const delay = getJitterMs();
            expect(delay).toBeGreaterThanOrEqual(0);
            expect(delay).toBeLessThan(30000);
        });

        it('returns 0 when maxMs is 0', () => {
            expect(getJitterMs(0)).toBe(0);
        });

        it('returns integer values (no decimals)', () => {
            for (let i = 0; i < 50; i++) {
                const delay = getJitterMs(10000);
                expect(Number.isInteger(delay)).toBe(true);
            }
        });
    });

    describe('randomJitter', () => {
        it('returns a promise that resolves after a delay', async () => {
            vi.spyOn(Math, 'random').mockReturnValue(0.5);

            const promise = randomJitter(10000);
            // With Math.random() = 0.5 and maxMs = 10000, delay = 5000ms
            vi.advanceTimersByTime(5000);
            await expect(promise).resolves.toBeUndefined();

            vi.restoreAllMocks();
        });

        it('resolves immediately when delay is 0', async () => {
            vi.spyOn(Math, 'random').mockReturnValue(0);

            const promise = randomJitter(10000);
            vi.advanceTimersByTime(0);
            await expect(promise).resolves.toBeUndefined();

            vi.restoreAllMocks();
        });

        it('does not resolve before the delay', async () => {
            vi.spyOn(Math, 'random').mockReturnValue(0.5);

            let resolved = false;
            randomJitter(10000).then(() => { resolved = true; });

            vi.advanceTimersByTime(4999);
            await Promise.resolve(); // flush microtasks
            expect(resolved).toBe(false);

            vi.advanceTimersByTime(1);
            await Promise.resolve();
            expect(resolved).toBe(true);

            vi.restoreAllMocks();
        });

        it('defaults to max 30000ms', async () => {
            vi.spyOn(Math, 'random').mockReturnValue(0.999);

            const promise = randomJitter();
            // 0.999 * 30000 = 29970
            vi.advanceTimersByTime(29970);
            await expect(promise).resolves.toBeUndefined();

            vi.restoreAllMocks();
        });
    });
});
