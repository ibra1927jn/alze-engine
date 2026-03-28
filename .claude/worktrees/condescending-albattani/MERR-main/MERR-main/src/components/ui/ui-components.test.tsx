/**
 * Tests for UI components: EmptyState, Toast, Button, StatusBadge, Icon, ModalOverlay, StatCard
 * Groups small, pure-rendering components into one test file for efficiency.
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, act } from '@testing-library/react';

// ── Icon ────────────────────────────────────────────
import Icon from './Icon';

describe('Icon', () => {
    it('renders icon name as text content', () => {
        render(<Icon name="shield" />);
        expect(screen.getByText('shield')).toBeTruthy();
    });

    it('applies custom size via inline style', () => {
        render(<Icon name="check" size={24} />);
        const el = screen.getByText('check');
        expect(el.style.fontSize).toBe('24px');
    });

    it('applies extra className', () => {
        render(<Icon name="star" className="text-yellow-500" />);
        expect(screen.getByText('star').className).toContain('text-yellow-500');
    });

    it('omits style when no size provided', () => {
        render(<Icon name="home" />);
        expect(screen.getByText('home').style.fontSize).toBe('');
    });
});

// ── EmptyState ──────────────────────────────────────
import EmptyState from './EmptyState';

describe('EmptyState', () => {
    it('renders title and icon', () => {
        render(<EmptyState icon="inbox" title="No items" />);
        expect(screen.getByText('No items')).toBeTruthy();
        expect(screen.getByText('inbox')).toBeTruthy();
    });

    it('renders subtitle when provided', () => {
        render(<EmptyState icon="inbox" title="Empty" subtitle="Nothing here yet" />);
        expect(screen.getByText('Nothing here yet')).toBeTruthy();
    });

    it('does not render subtitle when absent', () => {
        render(<EmptyState icon="inbox" title="Empty" />);
        expect(screen.queryByText('Nothing here yet')).toBeNull();
    });

    it('renders action button and fires onClick', () => {
        const onClick = vi.fn();
        render(<EmptyState icon="add" title="Empty" action={{ label: 'Add Item', onClick }} />);
        fireEvent.click(screen.getByText('Add Item'));
        expect(onClick).toHaveBeenCalledOnce();
    });

    it('renders action icon when provided', () => {
        render(<EmptyState icon="inbox" title="Empty" action={{ label: 'Create', onClick: vi.fn(), icon: 'add_circle' }} />);
        expect(screen.getByText('add_circle')).toBeTruthy();
    });
});

// ── Toast ───────────────────────────────────────────
import Toast from './Toast';

describe('Toast', () => {
    beforeEach(() => { vi.useFakeTimers(); });
    afterEach(() => { vi.useRealTimers(); });

    it('renders message text', () => {
        render(<Toast message="Saved!" onClose={vi.fn()} />);
        expect(screen.getByText('Saved!')).toBeTruthy();
    });

    it('renders correct icon for each type', () => {
        const { rerender } = render(<Toast message="ok" type="success" onClose={vi.fn()} />);
        expect(screen.getByText('check_circle')).toBeTruthy();
        rerender(<Toast message="fail" type="error" onClose={vi.fn()} />);
        expect(screen.getByText('error')).toBeTruthy();
    });

    it('auto-dismisses after 3 seconds', () => {
        const onClose = vi.fn();
        render(<Toast message="bye" onClose={onClose} />);
        expect(onClose).not.toHaveBeenCalled();
        act(() => { vi.advanceTimersByTime(3000); });
        expect(onClose).toHaveBeenCalledOnce();
    });

    it('calls onClose when close button clicked', () => {
        const onClose = vi.fn();
        render(<Toast message="msg" onClose={onClose} />);
        const closeBtn = screen.getByText('close').closest('button')!;
        fireEvent.click(closeBtn);
        expect(onClose).toHaveBeenCalledOnce();
    });
});

// ── Button ──────────────────────────────────────────
import { Button } from './Button';

describe('Button', () => {
    it('renders children text', () => {
        render(<Button>Click Me</Button>);
        expect(screen.getByText('Click Me')).toBeTruthy();
    });

    it('applies primary variant by default', () => {
        render(<Button>Test</Button>);
        const btn = screen.getByRole('button');
        expect(btn.className).toContain('bg-primary');
    });

    it('applies danger variant', () => {
        render(<Button variant="danger">Delete</Button>);
        expect(screen.getByRole('button').className).toContain('bg-danger');
    });

    it('shows spinner when loading', () => {
        render(<Button loading>Save</Button>);
        const btn = screen.getByRole('button');
        expect(btn.querySelector('.animate-spin')).toBeTruthy();
        expect(btn).toBeDisabled();
    });

    it('is disabled when disabled prop set', () => {
        render(<Button disabled>Nope</Button>);
        expect(screen.getByRole('button')).toBeDisabled();
    });

    it('renders icon when provided', () => {
        render(<Button icon={<span data-testid="icon">★</span>}>Star</Button>);
        expect(screen.getByTestId('icon')).toBeTruthy();
    });

    it('fires onClick handler', () => {
        const onClick = vi.fn();
        render(<Button onClick={onClick}>Go</Button>);
        fireEvent.click(screen.getByRole('button'));
        expect(onClick).toHaveBeenCalledOnce();
    });

    it('applies fullWidth class', () => {
        render(<Button fullWidth>Wide</Button>);
        expect(screen.getByRole('button').className).toContain('w-full');
    });
});

// ── StatusBadge ─────────────────────────────────────
import StatusBadge from './StatusBadge';

describe('StatusBadge', () => {
    it('renders label text', () => {
        render(<StatusBadge status="active" label="Online" />);
        expect(screen.getByText('Online')).toBeTruthy();
    });

    it('renders icon when provided', () => {
        render(<StatusBadge status="warning" label="Alert" icon="warning" />);
        expect(screen.getByText('warning')).toBeTruthy();
    });

    it('renders dot when no icon', () => {
        const { container } = render(<StatusBadge status="active" label="Active" />);
        const dot = container.querySelector('.rounded-full.bg-emerald-500');
        expect(dot).toBeTruthy();
    });

    it('applies danger styles', () => {
        render(<StatusBadge status="danger" label="Critical" />);
        expect(screen.getByText('Critical').className).toContain('bg-red-50');
    });

    it('applies sm size class', () => {
        render(<StatusBadge status="info" label="Info" size="sm" />);
        expect(screen.getByText('Info').className).toContain('text-xs');
    });
});

// ── StatCard ────────────────────────────────────────
import StatCard from './StatCard';

describe('StatCard', () => {
    it('renders value and label', () => {
        render(<StatCard icon="people" value="42" label="Total Pickers" />);
        expect(screen.getByText('42')).toBeTruthy();
        expect(screen.getByText('Total Pickers')).toBeTruthy();
    });

    it('renders icon', () => {
        render(<StatCard icon="agriculture" value="100" label="Bins" />);
        expect(screen.getByText('agriculture')).toBeTruthy();
    });

    it('renders trend when provided', () => {
        render(<StatCard icon="attach_money" value="$850" label="Revenue" trend={{ direction: 'up', value: '+12%' }} />);
        expect(screen.getByText('+12%')).toBeTruthy();
        expect(screen.getByText('trending_up')).toBeTruthy();
    });

    it('fires onClick when interactive', () => {
        const onClick = vi.fn();
        render(<StatCard icon="star" value="5" label="Rating" onClick={onClick} />);
        fireEvent.click(screen.getByRole('button'));
        expect(onClick).toHaveBeenCalledOnce();
    });

    it('has no button role when non-interactive', () => {
        render(<StatCard icon="star" value="5" label="Rating" />);
        expect(screen.queryByRole('button')).toBeNull();
    });
});

// ── ModalOverlay ────────────────────────────────────
import ModalOverlay from './ModalOverlay';

describe('ModalOverlay', () => {
    it('renders children inside dialog', () => {
        render(<ModalOverlay onClose={vi.fn()}><p>Modal Content</p></ModalOverlay>);
        expect(screen.getByText('Modal Content')).toBeTruthy();
    });

    it('has role dialog and aria-modal', () => {
        render(<ModalOverlay onClose={vi.fn()}><p>Test</p></ModalOverlay>);
        const dialog = screen.getByRole('dialog');
        expect(dialog).toBeTruthy();
        expect(dialog.getAttribute('aria-modal')).toBe('true');
    });

    it('calls onClose on Escape key', () => {
        const onClose = vi.fn();
        render(<ModalOverlay onClose={onClose}><p>Test</p></ModalOverlay>);
        fireEvent.keyDown(document, { key: 'Escape' });
        expect(onClose).toHaveBeenCalledOnce();
    });

    it('does NOT call onClose on Escape when static', () => {
        const onClose = vi.fn();
        render(<ModalOverlay onClose={onClose} isStatic><p>Test</p></ModalOverlay>);
        fireEvent.keyDown(document, { key: 'Escape' });
        expect(onClose).not.toHaveBeenCalled();
    });

    it('applies custom ariaLabel', () => {
        render(<ModalOverlay onClose={vi.fn()} ariaLabel="Confirm Delete"><p>Sure?</p></ModalOverlay>);
        expect(screen.getByLabelText('Confirm Delete')).toBeTruthy();
    });

    it('locks body scroll on mount', () => {
        render(<ModalOverlay onClose={vi.fn()}><p>X</p></ModalOverlay>);
        expect(document.body.style.overflow).toBe('hidden');
    });
});
