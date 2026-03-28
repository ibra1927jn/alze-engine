/**
 * Tests for auth components: Decorations, DemoAccess, LoginForm
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { VineLeaf, GrapeCluster, ParticleDots } from './login/Decorations';
import DemoAccess from './DemoAccess';
import LoginForm from './LoginForm';

describe('Decorations', () => {
    it('VineLeaf renders SVG', () => {
        const { container } = render(<VineLeaf />);
        expect(container.querySelector('svg')).toBeTruthy();
    });

    it('VineLeaf accepts className', () => {
        const { container } = render(<VineLeaf className="test-class" />);
        expect(container.querySelector('svg')?.classList.contains('test-class')).toBe(true);
    });

    it('GrapeCluster renders SVG with circles', () => {
        const { container } = render(<GrapeCluster />);
        const circles = container.querySelectorAll('circle');
        expect(circles.length).toBe(6);
    });

    it('ParticleDots renders 18 particles', () => {
        const { container } = render(<ParticleDots />);
        const dots = container.querySelectorAll('.rounded-full');
        expect(dots.length).toBe(18);
    });
});

describe('DemoAccess', () => {
    it('renders all 8 demo role buttons', () => {
        render(<DemoAccess isSubmitting={false} onDemoAccess={vi.fn()} />);
        expect(screen.getByText('Manager')).toBeTruthy();
        expect(screen.getByText('Team Leader')).toBeTruthy();
        expect(screen.getByText('Bucket Runner')).toBeTruthy();
        expect(screen.getByText('QC Inspector')).toBeTruthy();
        expect(screen.getByText('Payroll Admin')).toBeTruthy();
        expect(screen.getByText('Admin')).toBeTruthy();
        expect(screen.getByText('HR Admin')).toBeTruthy();
        expect(screen.getByText('Logistics')).toBeTruthy();
    });

    it('calls onDemoAccess with correct role', () => {
        const onDemoAccess = vi.fn();
        render(<DemoAccess isSubmitting={false} onDemoAccess={onDemoAccess} />);
        fireEvent.click(screen.getByText('Manager'));
        expect(onDemoAccess).toHaveBeenCalledWith('manager');
    });

    it('disables buttons when submitting', () => {
        render(<DemoAccess isSubmitting={true} onDemoAccess={vi.fn()} />);
        const buttons = screen.getAllByRole('button');
        buttons.forEach(btn => expect(btn).toBeDisabled());
    });

    it('shows role descriptions', () => {
        render(<DemoAccess isSubmitting={false} onDemoAccess={vi.fn()} />);
        expect(screen.getByText('Command center & analytics')).toBeTruthy();
        expect(screen.getByText('Manage pickers & rows')).toBeTruthy();
    });
});

describe('LoginForm', () => {
    const defaultProps = {
        email: '', setEmail: vi.fn(),
        password: '', setPassword: vi.fn(),
        isSubmitting: false,
        onSubmit: vi.fn(),
        onForgotPassword: vi.fn(),
    };

    it('renders email and password fields', () => {
        render(<LoginForm {...defaultProps} />);
        expect(screen.getByPlaceholderText('you@email.com')).toBeTruthy();
        expect(screen.getByPlaceholderText('••••••••')).toBeTruthy();
    });

    it('renders Sign In button', () => {
        render(<LoginForm {...defaultProps} />);
        expect(screen.getByText('Sign In')).toBeTruthy();
    });

    it('calls setEmail on input change', () => {
        const setEmail = vi.fn();
        render(<LoginForm {...defaultProps} setEmail={setEmail} />);
        fireEvent.change(screen.getByPlaceholderText('you@email.com'), { target: { value: 'test@test.com' } });
        expect(setEmail).toHaveBeenCalledWith('test@test.com');
    });

    it('calls setPassword on input change', () => {
        const setPassword = vi.fn();
        render(<LoginForm {...defaultProps} setPassword={setPassword} />);
        fireEvent.change(screen.getByPlaceholderText('••••••••'), { target: { value: 'pass123' } });
        expect(setPassword).toHaveBeenCalledWith('pass123');
    });

    it('calls onSubmit on form submit', () => {
        const onSubmit = vi.fn(e => e.preventDefault());
        render(<LoginForm {...defaultProps} onSubmit={onSubmit} />);
        fireEvent.submit(screen.getByText('Sign In').closest('form')!);
        expect(onSubmit).toHaveBeenCalled();
    });

    it('calls onForgotPassword when clicked', () => {
        const onForgot = vi.fn();
        render(<LoginForm {...defaultProps} onForgotPassword={onForgot} />);
        fireEvent.click(screen.getByText('Forgot your password?'));
        expect(onForgot).toHaveBeenCalled();
    });

    it('disables submit when isSubmitting', () => {
        render(<LoginForm {...defaultProps} isSubmitting={true} />);
        expect(screen.getByText('Sign In').closest('button')).toBeDisabled();
    });

    it('toggles password visibility', () => {
        render(<LoginForm {...defaultProps} />);
        const passwordInput = screen.getByPlaceholderText('••••••••');
        expect(passwordInput).toHaveAttribute('type', 'password');
        // Find the visibility toggle button
        const toggleBtn = screen.getByText('visibility').closest('button')!;
        fireEvent.click(toggleBtn);
        expect(passwordInput).toHaveAttribute('type', 'text');
    });
});
