/**
 * Drawer — Slide-from-right panel (native-feeling)
 * 
 * Uses React Portal to render at document root (no z-index battles).
 * Locks body scroll when open per pro-tip #4.
 */
import React, { useEffect, useCallback, useRef } from 'react';
import { createPortal } from 'react-dom';
import { cn } from '@/utils/cn';

interface DrawerProps {
    /** Whether the drawer is open */
    isOpen: boolean;
    /** Close handler */
    onClose: () => void;
    /** Panel title */
    title?: string;
    /** Title icon name */
    icon?: string;
    /** Panel width class */
    width?: string;
    /** Content */
    children: React.ReactNode;
    /** Extra panel classes */
    className?: string;
}

const Drawer: React.FC<DrawerProps> = ({
    isOpen,
    onClose,
    title,
    icon,
    width = 'max-w-md',
    children,
    className,
}) => {
    const panelRef = useRef<HTMLDivElement>(null);

    // Escape key close
    const handleEscape = useCallback((e: KeyboardEvent) => {
        if (e.key === 'Escape') onClose();
    }, [onClose]);

    // Lock body scroll & register escape listener
    useEffect(() => {
        if (!isOpen) return;
        const originalOverflow = document.body.style.overflow;
        document.body.style.overflow = 'hidden';
        document.addEventListener('keydown', handleEscape);
        panelRef.current?.focus();

        return () => {
            document.body.style.overflow = originalOverflow;
            document.removeEventListener('keydown', handleEscape);
        };
    }, [isOpen, handleEscape]);

    if (!isOpen) return null;

    const drawer = (
        <div
            className="fixed inset-0 z-[200] flex justify-end"
            aria-modal="true"
            role="dialog"
        >
            {/* Backdrop */}
            <div
                className="absolute inset-0 bg-black/40 backdrop-blur-sm animate-fade-in"
                onClick={onClose}
            />

            {/* Panel */}
            <div
                ref={panelRef}
                tabIndex={-1}
                className={cn(
                    'relative w-full h-full bg-white shadow-2xl outline-none',
                    'animate-slide-in-right overflow-y-auto',
                    width,
                    className,
                )}
            >
                {/* Header */}
                {title && (
                    <div className="sticky top-0 z-10 flex items-center gap-3 px-5 py-4 bg-white border-b border-slate-200">
                        {icon && (
                            <span className="material-symbols-outlined text-indigo-600 text-xl">{icon}</span>
                        )}
                        <h2 className="text-lg font-bold text-slate-900 flex-1">{title}</h2>
                        <button
                            onClick={onClose}
                            className="w-8 h-8 rounded-full flex items-center justify-center text-slate-400 hover:text-slate-600 hover:bg-slate-100 transition-colors"
                            aria-label="Close panel"
                        >
                            <span className="material-symbols-outlined text-xl">close</span>
                        </button>
                    </div>
                )}

                {/* Content */}
                <div className={cn(!title && 'pt-4')}>
                    {children}
                </div>
            </div>
        </div>
    );

    // Render via Portal to avoid z-index issues with BottomNav
    return createPortal(drawer, document.body);
};

export default React.memo(Drawer);
