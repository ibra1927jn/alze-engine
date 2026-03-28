import { describe, it, expect, vi, beforeAll, afterAll } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import ErrorBoundary from '../ErrorBoundary';
import React from 'react';

// Silence console.error during test runs for the intentional throws
const originalError = console.error;
beforeAll(() => { console.error = vi.fn(); });
afterAll(() => { console.error = originalError; });

// Component that throws an error
const ThrowingChild = ({ shouldThrow = true }: { shouldThrow?: boolean }) => {
    if (shouldThrow) throw new Error('Test crash');
    return <div>Happy path content</div>;
};

describe('ErrorBoundary', () => {
    it('renders children when there is no error', () => {
        render(
            <ErrorBoundary>
                <div>Safe content</div>
            </ErrorBoundary>
        );
        expect(screen.getByText('Safe content')).toBeInTheDocument();
    });

    it('renders error fallback when child throws', () => {
        render(
            <ErrorBoundary>
                <ThrowingChild />
            </ErrorBoundary>
        );

        expect(screen.getByText('Something went wrong')).toBeInTheDocument();
        expect(screen.getByText(/Test crash/)).toBeInTheDocument();
        expect(screen.getByText('Reload Application')).toBeInTheDocument();
        expect(screen.getByText('Clear Cache & Reload')).toBeInTheDocument();
    });

    it('shows the Reload Application button that triggers reload', () => {
        const reloadMock = vi.fn();
        Object.defineProperty(window, 'location', {
            value: { reload: reloadMock },
            writable: true,
        });

        render(
            <ErrorBoundary>
                <ThrowingChild />
            </ErrorBoundary>
        );

        fireEvent.click(screen.getByText('Reload Application'));
        expect(reloadMock).toHaveBeenCalled();
    });

    it('displays error message in code block', () => {
        render(
            <ErrorBoundary>
                <ThrowingChild />
            </ErrorBoundary>
        );

        const errorCode = screen.getByText(/Error: Test crash/);
        expect(errorCode.tagName).toBe('CODE');
    });
});
