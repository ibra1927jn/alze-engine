/**
 * usePhotoCapture — Camera/file capture hook for QC photo evidence
 *
 * - Opens device camera (mobile) or file picker (desktop)
 * - Compresses image via canvas to configurable max size
 * - Returns blob + preview URL ready for upload
 */
import { logger } from '@/utils/logger';
import { useState, useRef, useCallback } from 'react';

interface UsePhotoCaptureOptions {
    maxSizePx?: number;   // Max width/height in pixels (default 1200)
    quality?: number;     // JPEG quality 0-1 (default 0.75)
}

interface UsePhotoCaptureReturn {
    /** Trigger native file/camera picker */
    capturePhoto: () => void;
    /** The compressed image blob, ready for upload */
    photoBlob: Blob | null;
    /** Object URL for preview (revoked on clear) */
    photoPreview: string | null;
    /** True while compressing */
    isCapturing: boolean;
    /** Clear the current photo */
    clearPhoto: () => void;
}

export function usePhotoCapture(options?: UsePhotoCaptureOptions): UsePhotoCaptureReturn {
    const { maxSizePx = 1200, quality = 0.75 } = options || {};

    const [photoBlob, setPhotoBlob] = useState<Blob | null>(null);
    const [photoPreview, setPhotoPreview] = useState<string | null>(null);
    const [isCapturing, setIsCapturing] = useState(false);
    const inputRef = useRef<HTMLInputElement | null>(null);

    const clearPhoto = useCallback(() => {
        if (photoPreview) {
            URL.revokeObjectURL(photoPreview);
        }
        setPhotoBlob(null);
        setPhotoPreview(null);
    }, [photoPreview]);

    const compressImage = useCallback((file: File): Promise<Blob> => {
        return new Promise((resolve, reject) => {
            const img = new Image();
            const url = URL.createObjectURL(file);

            img.onload = () => {
                URL.revokeObjectURL(url);

                // Calculate scaled dimensions
                let { width, height } = img;
                if (width > maxSizePx || height > maxSizePx) {
                    const ratio = Math.min(maxSizePx / width, maxSizePx / height);
                    width = Math.round(width * ratio);
                    height = Math.round(height * ratio);
                }

                // Draw to canvas
                const canvas = document.createElement('canvas');
                canvas.width = width;
                canvas.height = height;
                const ctx = canvas.getContext('2d');
                if (!ctx) {
                    reject(new Error('Canvas context unavailable'));
                    return;
                }
                ctx.drawImage(img, 0, 0, width, height);

                // Export as WebP (fallback to JPEG)
                canvas.toBlob(
                    (blob) => {
                        if (blob) {
                            resolve(blob);
                        } else {
                            reject(new Error('Canvas toBlob failed'));
                        }
                    },
                    'image/webp',
                    quality,
                );
            };

            img.onerror = () => {
                URL.revokeObjectURL(url);
                reject(new Error('Failed to load image'));
            };

            img.src = url;
        });
    }, [maxSizePx, quality]);

    const handleFileChange = useCallback(async (e: Event) => {
        const input = e.target as HTMLInputElement;
        const file = input.files?.[0];
        if (!file) return;

        setIsCapturing(true);
        try {
            const compressed = await compressImage(file);
            // Revoke old preview
            if (photoPreview) {
                URL.revokeObjectURL(photoPreview);
            }
            const preview = URL.createObjectURL(compressed);
            setPhotoBlob(compressed);
            setPhotoPreview(preview);
        } catch (err) {
            logger.error('[usePhotoCapture] Compression failed:', err);
        } finally {
            setIsCapturing(false);
            // Reset input so same file can be re-selected
            input.value = '';
        }
    }, [compressImage, photoPreview]);

    const capturePhoto = useCallback(() => {
        // Create a hidden input each time (avoids stale refs)
        if (inputRef.current) {
            inputRef.current.removeEventListener('change', handleFileChange);
            inputRef.current.remove();
        }

        const input = document.createElement('input');
        input.type = 'file';
        input.accept = 'image/*';
        input.capture = 'environment'; // Rear camera on mobile
        input.style.display = 'none';
        input.addEventListener('change', handleFileChange);
        document.body.appendChild(input);
        inputRef.current = input;

        input.click();
    }, [handleFileChange]);

    return { capturePhoto, photoBlob, photoPreview, isCapturing, clearPhoto };
}
