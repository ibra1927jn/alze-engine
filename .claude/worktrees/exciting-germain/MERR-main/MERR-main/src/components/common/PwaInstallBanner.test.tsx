/**
 * Tests for PwaInstallBanner — PWA install prompt banner
 * @module components/common/PwaInstallBanner.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

const mocks = vi.hoisted(() => ({
    canInstall: true,
    isInstalled: false,
    isDismissed: false,
    promptInstall: vi.fn(),
    dismissBanner: vi.fn(),
}));

vi.mock('@/hooks/usePwaInstall', () => ({
    usePwaInstall: () => ({
        canInstall: mocks.canInstall,
        isInstalled: mocks.isInstalled,
        isDismissed: mocks.isDismissed,
        promptInstall: mocks.promptInstall,
        dismissBanner: mocks.dismissBanner,
    }),
}));

import PwaInstallBanner from './PwaInstallBanner';

describe('PwaInstallBanner', () => {
    beforeEach(() => {
        vi.clearAllMocks();
        mocks.canInstall = true;
        mocks.isInstalled = false;
        mocks.isDismissed = false;
    });

    it('renders when installable', () => {
        render(<PwaInstallBanner />);
        expect(screen.getByText('Install HarvestPro')).toBeTruthy();
    });

    it('renders Install button', () => {
        render(<PwaInstallBanner />);
        expect(screen.getByText('Install')).toBeTruthy();
    });

    it('calls promptInstall on Install click', () => {
        render(<PwaInstallBanner />);
        fireEvent.click(screen.getByText('Install'));
        expect(mocks.promptInstall).toHaveBeenCalled();
    });

    it('calls dismissBanner on close click', () => {
        render(<PwaInstallBanner />);
        const closeBtn = screen.getByLabelText('Dismiss install banner');
        fireEvent.click(closeBtn);
        expect(mocks.dismissBanner).toHaveBeenCalled();
    });

    it('does not render when already installed', () => {
        mocks.isInstalled = true;
        const { container } = render(<PwaInstallBanner />);
        expect(container.innerHTML).toBe('');
    });

    it('does not render when dismissed', () => {
        mocks.isDismissed = true;
        const { container } = render(<PwaInstallBanner />);
        expect(container.innerHTML).toBe('');
    });

    it('does not render when cannot install', () => {
        mocks.canInstall = false;
        const { container } = render(<PwaInstallBanner />);
        expect(container.innerHTML).toBe('');
    });

    it('shows offline capabilities text', () => {
        render(<PwaInstallBanner />);
        expect(screen.getByText(/Works offline/)).toBeTruthy();
    });
});
