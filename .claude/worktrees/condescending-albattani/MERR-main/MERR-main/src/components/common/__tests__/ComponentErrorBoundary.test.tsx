import { describe, it, expect, vi, beforeAll, afterAll } from 'vitest';
import { render, screen } from '@testing-library/react';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';
import React from 'react';

// Silence console.error during test runs
const originalError = console.error;
beforeAll(() => { console.error = vi.fn(); });
afterAll(() => { console.error = originalError; });

const ThrowingWidget = () => { throw new Error('Widget crash'); };
const WorkingWidget = () => <div>Widget OK</div>;

describe('ComponentErrorBoundary', () => {
    it('renders children when no error', () => {
        render(
            <ComponentErrorBoundary componentName="TestWidget">
                <WorkingWidget />
            </ComponentErrorBoundary>
        );
        expect(screen.getByText('Widget OK')).toBeInTheDocument();
    });

    it('renders inline error card when child throws', () => {
        render(
            <ComponentErrorBoundary componentName="BrokenWidget">
                <ThrowingWidget />
            </ComponentErrorBoundary>
        );

        expect(screen.getByText('BrokenWidget failed to load')).toBeInTheDocument();
    });

    it('shows retry button in error state', () => {
        render(
            <ComponentErrorBoundary componentName="CrashWidget">
                <ThrowingWidget />
            </ComponentErrorBoundary>
        );

        expect(screen.getByText('Retry')).toBeInTheDocument();
    });

    it('does not crash the entire page — sibling survives', () => {
        render(
            <div>
                <ComponentErrorBoundary componentName="BrokenOne">
                    <ThrowingWidget />
                </ComponentErrorBoundary>
                <div>Still alive</div>
            </div>
        );

        expect(screen.getByText('Still alive')).toBeInTheDocument();
        expect(screen.getByText('BrokenOne failed to load')).toBeInTheDocument();
    });
});
