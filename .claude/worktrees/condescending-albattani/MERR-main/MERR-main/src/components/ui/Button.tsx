/**
 * ============================================
 * RUGGED BUTTON — Field-Ready Component
 * ============================================
 * Big, tactile buttons designed for gloved hands,
 * bright sun, and high-stress orchard environments.
 * ============================================
 */
import React from 'react';

type ButtonVariant = 'primary' | 'success' | 'danger' | 'ghost' | 'info';
type ButtonSize = 'md' | 'lg';

interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
    variant?: ButtonVariant;
    size?: ButtonSize;
    icon?: React.ReactNode;
    loading?: boolean;
    fullWidth?: boolean;
}

const variantClasses: Record<ButtonVariant, string> = {
    primary:
        'bg-primary text-white hover:bg-primary-dim shadow-glow hover:shadow-lg active:shadow-md',
    success:
        'bg-success text-white hover:bg-emerald-600 shadow-glow-success hover:shadow-lg active:shadow-md',
    danger:
        'bg-danger text-white hover:bg-red-600 shadow-md hover:shadow-lg active:shadow-md',
    info:
        'bg-info text-white hover:bg-info-dim shadow-glow-info hover:shadow-lg active:shadow-md',
    ghost:
        'bg-transparent text-text-main border-2 border-border-light hover:bg-background-light active:bg-surface-secondary',
};

const sizeClasses: Record<ButtonSize, string> = {
    md: 'min-h-[48px] px-6 py-3 text-sm',
    lg: 'min-h-[56px] px-8 py-4 text-base',
};

export const Button: React.FC<ButtonProps> = ({
    children,
    variant = 'primary',
    size = 'md',
    icon,
    loading = false,
    fullWidth = false,
    disabled,
    className = '',
    ...props
}) => {
    return (
        <button
            className={`
                inline-flex items-center justify-center gap-2.5
                font-semibold font-display
                rounded-xl
                transition-all duration-200 ease-out
                hover:scale-[1.02] active:scale-[0.98]
                disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:scale-100
                focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-primary/50
                ${variantClasses[variant]}
                ${sizeClasses[size]}
                ${fullWidth ? 'w-full' : ''}
                ${className}
            `.trim()}
            disabled={disabled || loading}
            {...props}
        >
            {loading ? (
                <div className="w-5 h-5 border-2 border-white/30 border-t-white rounded-full animate-spin" />
            ) : icon ? (
                <span className="w-5 h-5 flex-shrink-0">{icon}</span>
            ) : null}
            {children}
        </button>
    );
};

export default Button;
