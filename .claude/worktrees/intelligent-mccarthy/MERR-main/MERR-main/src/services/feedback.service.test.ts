/**
 * feedback.service.test.ts — Unit tests for haptic/audio feedback service
 *
 * The FeedbackService constructor checks `typeof window !== 'undefined'`
 * and reads `window.AudioContext`. Since jsdom provides window but no AudioContext,
 * the service's audioContext will be null unless we provide it.
 *
 * Instead of fighting module loading order, we:
 * 1. Test vibrate() directly (doesn't need AudioContext)
 * 2. For beep tests, manually set the audioContext after import
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { feedbackService } from './feedback.service';

// Track oscillator type mutations
let lastOscillatorType = '';

function createMockAudioCtx() {
    return {
        state: 'running' as string,
        currentTime: 0,
        destination: {},
        resume: vi.fn(),
        createOscillator: vi.fn(() => ({
            connect: vi.fn(),
            get type() { return lastOscillatorType; },
            set type(v: OscillatorType) { lastOscillatorType = v; },
            frequency: { setValueAtTime: vi.fn(), exponentialRampToValueAtTime: vi.fn() },
            start: vi.fn(),
            stop: vi.fn(),
        })),
        createGain: vi.fn(() => ({
            connect: vi.fn(),
            gain: {
                setValueAtTime: vi.fn(),
                exponentialRampToValueAtTime: vi.fn(),
                linearRampToValueAtTime: vi.fn(),
            },
        })),
    };
}

let mockCtx: ReturnType<typeof createMockAudioCtx>;

describe('FeedbackService', () => {
    beforeEach(() => {
        vi.clearAllMocks();
        lastOscillatorType = '';
        // Inject a fresh mock AudioContext into the service
        mockCtx = createMockAudioCtx();
        (feedbackService as unknown as { audioContext: unknown }).audioContext = mockCtx;
    });

    describe('vibrate', () => {
        it('calls navigator.vibrate with default 200ms', () => {
            const spy = vi.fn();
            vi.stubGlobal('navigator', { vibrate: spy });
            feedbackService.vibrate();
            expect(spy).toHaveBeenCalledWith(200);
        });

        it('calls navigator.vibrate with custom pattern', () => {
            const spy = vi.fn();
            vi.stubGlobal('navigator', { vibrate: spy });
            feedbackService.vibrate([100, 50, 100]);
            expect(spy).toHaveBeenCalledWith([100, 50, 100]);
        });

        it('does not throw if vibrate unavailable', () => {
            vi.stubGlobal('navigator', {});
            expect(() => feedbackService.vibrate()).not.toThrow();
        });
    });

    describe('beep', () => {
        it('uses sine oscillator for success', () => {
            feedbackService.beep('success');
            expect(mockCtx.createOscillator).toHaveBeenCalled();
            expect(lastOscillatorType).toBe('sine');
        });

        it('uses sawtooth oscillator for error', () => {
            feedbackService.beep('error');
            expect(lastOscillatorType).toBe('sawtooth');
        });

        it('resumes context if suspended', () => {
            mockCtx.state = 'suspended';
            feedbackService.beep('success');
            expect(mockCtx.resume).toHaveBeenCalled();
        });

        it('does nothing when audioContext is null', () => {
            (feedbackService as unknown as { audioContext: unknown }).audioContext = null;
            expect(() => feedbackService.beep('success')).not.toThrow();
            expect(mockCtx.createOscillator).not.toHaveBeenCalled();
        });
    });

    describe('triggerSuccess', () => {
        it('vibrates double-tap and plays sine beep', () => {
            const spy = vi.fn();
            vi.stubGlobal('navigator', { vibrate: spy });
            feedbackService.triggerSuccess();
            expect(spy).toHaveBeenCalledWith([100, 50, 100]);
            expect(lastOscillatorType).toBe('sine');
        });
    });

    describe('triggerError', () => {
        it('vibrates long and plays sawtooth beep', () => {
            const spy = vi.fn();
            vi.stubGlobal('navigator', { vibrate: spy });
            feedbackService.triggerError();
            expect(spy).toHaveBeenCalledWith([400]);
            expect(lastOscillatorType).toBe('sawtooth');
        });
    });
});
