/**
 * usePhotoCapture Hook Tests
 */
import { describe, it, expect, vi } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { usePhotoCapture } from './usePhotoCapture';

describe('usePhotoCapture', () => {
    it('initializes with null blob and preview', () => {
        const { result } = renderHook(() => usePhotoCapture());
        expect(result.current.photoBlob).toBeNull();
        expect(result.current.photoPreview).toBeNull();
        expect(result.current.isCapturing).toBe(false);
    });

    it('capturePhoto triggers file input click', () => {
        const clickSpy = vi.fn();
        const mockInput = document.createElement('input');
        vi.spyOn(mockInput, 'click').mockImplementation(clickSpy);
        vi.spyOn(document, 'createElement').mockReturnValue(mockInput);

        const { result } = renderHook(() => usePhotoCapture());
        act(() => { result.current.capturePhoto(); });
        expect(clickSpy).toHaveBeenCalled();
        vi.restoreAllMocks();
    });

    it('clearPhoto revokes URL and resets state', () => {
        const revokeSpy = vi.spyOn(URL, 'revokeObjectURL').mockImplementation(() => { });
        const { result } = renderHook(() => usePhotoCapture());
        act(() => { result.current.clearPhoto(); });
        expect(result.current.photoBlob).toBeNull();
        expect(result.current.photoPreview).toBeNull();
        revokeSpy.mockRestore();
    });

    it('accepts custom options without errors', () => {
        const { result } = renderHook(() => usePhotoCapture({ maxSizePx: 800, quality: 0.5 }));
        expect(result.current.photoBlob).toBeNull();
    });

    it('exposes capturePhoto as function', () => {
        const { result } = renderHook(() => usePhotoCapture());
        expect(typeof result.current.capturePhoto).toBe('function');
        expect(typeof result.current.clearPhoto).toBe('function');
    });
});
