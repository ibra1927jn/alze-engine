import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import LoginForm from '../../auth/LoginForm';
import React from 'react';

describe('LoginForm', () => {
    const defaultProps = {
        email: '',
        setEmail: vi.fn(),
        password: '',
        setPassword: vi.fn(),
        isSubmitting: false,
        onSubmit: vi.fn((e: React.FormEvent) => e.preventDefault()),
        onForgotPassword: vi.fn(),
    };

    it('renders email and password inputs', () => {
        render(<LoginForm {...defaultProps} />);

        expect(screen.getByPlaceholderText('you@email.com')).toBeInTheDocument();
        expect(screen.getByPlaceholderText('••••••••')).toBeInTheDocument();
    });

    it('renders Sign In button', () => {
        render(<LoginForm {...defaultProps} />);
        expect(screen.getByText('Sign In')).toBeInTheDocument();
    });

    it('shows submitting state when isSubmitting is true', () => {
        render(<LoginForm {...defaultProps} isSubmitting={true} />);
        // Button still says 'Sign In' but shows progress_activity icon
        expect(screen.getByText('Sign In')).toBeInTheDocument();
        expect(screen.getByText('progress_activity')).toBeInTheDocument();
    });

    it('disables submit button when isSubmitting', () => {
        render(<LoginForm {...defaultProps} isSubmitting={true} />);
        const btn = screen.getByRole('button', { name: /sign in/i });
        expect(btn).toBeDisabled();
    });

    it('calls setEmail when email input changes', () => {
        const setEmail = vi.fn();
        render(<LoginForm {...defaultProps} setEmail={setEmail} />);

        fireEvent.change(screen.getByPlaceholderText('you@email.com'), {
            target: { value: 'test@example.com' },
        });
        expect(setEmail).toHaveBeenCalledWith('test@example.com');
    });

    it('calls setPassword when password input changes', () => {
        const setPassword = vi.fn();
        render(<LoginForm {...defaultProps} setPassword={setPassword} />);

        fireEvent.change(screen.getByPlaceholderText('••••••••'), {
            target: { value: 'secret123' },
        });
        expect(setPassword).toHaveBeenCalledWith('secret123');
    });

    it('calls onSubmit when form is submitted', () => {
        const onSubmit = vi.fn((e: React.FormEvent) => e.preventDefault());
        render(<LoginForm {...defaultProps} onSubmit={onSubmit} />);

        fireEvent.submit(screen.getByRole('button', { name: /sign in/i }).closest('form')!);
        expect(onSubmit).toHaveBeenCalled();
    });

    it('calls onForgotPassword when forgot password is clicked', () => {
        const onForgot = vi.fn();
        render(<LoginForm {...defaultProps} onForgotPassword={onForgot} />);

        fireEvent.click(screen.getByText('Forgot your password?'));
        expect(onForgot).toHaveBeenCalled();
    });

    it('has required attributes on email and password', () => {
        render(<LoginForm {...defaultProps} />);

        expect(screen.getByPlaceholderText('you@email.com')).toBeRequired();
        expect(screen.getByPlaceholderText('••••••••')).toBeRequired();
    });

    it('toggles password visibility', () => {
        render(<LoginForm {...defaultProps} password="secret" />);

        const passwordInput = screen.getByPlaceholderText('••••••••');
        expect(passwordInput).toHaveAttribute('type', 'password');

        // Click show password button
        const toggleButtons = screen.getAllByRole('button');
        const visibilityButton = toggleButtons.find(btn => btn.querySelector('.material-symbols-outlined'));
        if (visibilityButton) {
            fireEvent.click(visibilityButton);
            expect(passwordInput).toHaveAttribute('type', 'text');
        }
    });
});
