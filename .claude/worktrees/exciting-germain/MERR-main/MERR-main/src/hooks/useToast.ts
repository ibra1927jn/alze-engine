import { useState, useCallback } from 'react';

export type ToastType = 'success' | 'error' | 'info' | 'warning';

interface ToastState {
    message: string;
    type: ToastType;
}

/**
 * useToast — Lightweight hook to replace native alert() calls.
 * 
 * Usage:
 *   const { toast, showToast, hideToast } = useToast();
 *   showToast('Saved!', 'success');
 *   {toast && <Toast message={toast.message} type={toast.type} onClose={hideToast} />}
 */
export function useToast() {
    const [toast, setToast] = useState<ToastState | null>(null);

    const showToast = useCallback((message: string, type: ToastType = 'info') => {
        setToast({ message, type });
    }, []);

    const hideToast = useCallback(() => {
        setToast(null);
    }, []);

    return { toast, showToast, hideToast } as const;
}
