import React, { useEffect } from 'react';

interface ToastProps {
    message: string;
    type?: 'success' | 'error' | 'info' | 'warning';
    onClose: () => void;
    className?: string;
}

const Toast: React.FC<ToastProps> = ({ message, type = 'info', onClose, className = '' }) => {
    useEffect(() => {
        const timer = setTimeout(() => {
            onClose();
        }, 3000); // Auto-dismiss after 3s

        return () => clearTimeout(timer);
    }, [onClose]);

    const bgColors = {
        success: 'bg-green-600',
        error: 'bg-red-600',
        info: 'bg-blue-600',
        warning: 'bg-orange-500'
    };

    const icons = {
        success: 'check_circle',
        error: 'error',
        info: 'info',
        warning: 'warning'
    };

    return (
        <div className={`fixed top-4 left-1/2 transform -translate-x-1/2 z-50 animate-bounce-in ${className}`}>
            <div className={`${bgColors[type]} text-white px-6 py-3 rounded-full shadow-lg flex items-center gap-3 min-w-[300px]`}>
                <span className="material-symbols-outlined">{icons[type]}</span>
                <span className="font-bold text-sm tracking-wide flex-1">{message}</span>
                <button onClick={onClose} className="opacity-80 hover:opacity-100">
                    <span className="material-symbols-outlined text-sm">close</span>
                </button>
            </div>
        </div>
    );
};

export default Toast;
