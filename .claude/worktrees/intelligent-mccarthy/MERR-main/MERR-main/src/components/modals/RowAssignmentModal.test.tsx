/**
 * RowAssignmentModal.test.tsx
 * Integration tests for the row assignment modal
 * 
 * Tests cover:
 * - Rendering pickers and form elements
 * - Picker selection toggle
 * - Validation (disables submit when row or pickers missing)
 * - Successful assignment flow
 * - Error handling with toast
 * - Empty state when no active pickers
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import RowAssignmentModal from './RowAssignmentModal';
import type { PickerForAssignment } from './RowAssignmentModal';

// ── Mock useToast ──────────────────────────
const mockShowToast = vi.fn();
vi.mock('@/hooks/useToast', () => ({
    useToast: () => ({
        toast: null,
        showToast: mockShowToast,
        hideToast: vi.fn(),
    }),
}));

// ── Mock ModalOverlay — render children directly ──
vi.mock('@/components/ui/ModalOverlay', () => ({
    default: ({ children, onClose }: { children: React.ReactNode; onClose: () => void }) => (
        <div data-testid="modal-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
            {children}
        </div>
    ),
}));

// ── Test Data ──────────────────────────────
const mockPickers: PickerForAssignment[] = [
    { id: 'p1', name: 'Alice Johnson', avatar: 'AJ', idNumber: 'PK001', status: 'Active' },
    { id: 'p2', name: 'Bob Smith', avatar: 'BS', idNumber: 'PK002', status: 'Active' },
    { id: 'p3', name: 'Charlie Brown', avatar: 'CB', idNumber: 'PK003', status: 'Break' },
    { id: 'p4', name: 'Diana Off', avatar: 'DO', idNumber: 'PK004', status: 'Off Duty' },
];

describe('RowAssignmentModal', () => {
    const mockOnClose = vi.fn();
    const mockOnAssign = vi.fn();

    beforeEach(() => {
        vi.clearAllMocks();
        mockOnAssign.mockResolvedValue(undefined);
    });

    const renderModal = (pickers = mockPickers) => {
        const user = userEvent.setup();
        render(
            <RowAssignmentModal
                onClose={mockOnClose}
                onAssign={mockOnAssign}
                pickers={pickers}
            />
        );
        return { user };
    };

    // ═══════════════════════════════════════
    // RENDERING
    // ═══════════════════════════════════════

    it('renders the modal with title and form elements', () => {
        renderModal();

        expect(screen.getByRole('heading', { name: 'Assign Row' })).toBeInTheDocument();
        expect(screen.getByPlaceholderText('12')).toBeInTheDocument();
        expect(screen.getByLabelText('Side')).toBeInTheDocument();
    });

    it('renders only active pickers (excludes Off Duty)', () => {
        renderModal();

        // Active pickers should appear
        expect(screen.getByText('Alice Johnson')).toBeInTheDocument();
        expect(screen.getByText('Bob Smith')).toBeInTheDocument();
        expect(screen.getByText('Charlie Brown')).toBeInTheDocument();

        // Off Duty picker should NOT appear
        expect(screen.queryByText('Diana Off')).not.toBeInTheDocument();
    });

    it('shows picker count in section header', () => {
        renderModal();

        expect(screen.getByText('Assign Pickers (0)')).toBeInTheDocument();
    });

    // ═══════════════════════════════════════
    // PICKER SELECTION
    // ═══════════════════════════════════════

    it('toggles picker selection via checkbox', async () => {
        const { user } = renderModal();

        const checkboxes = screen.getAllByRole('checkbox');
        expect(checkboxes).toHaveLength(3); // 3 active pickers

        // Select first picker
        await user.click(checkboxes[0]);
        expect(checkboxes[0]).toBeChecked();
        expect(screen.getByText('Assign Pickers (1)')).toBeInTheDocument();

        // Select second picker
        await user.click(checkboxes[1]);
        expect(checkboxes[1]).toBeChecked();
        expect(screen.getByText('Assign Pickers (2)')).toBeInTheDocument();

        // Deselect first picker
        await user.click(checkboxes[0]);
        expect(checkboxes[0]).not.toBeChecked();
        expect(screen.getByText('Assign Pickers (1)')).toBeInTheDocument();
    });

    // ═══════════════════════════════════════
    // VALIDATION
    // ═══════════════════════════════════════

    it('submit button is disabled when row number is empty', async () => {
        const { user } = renderModal();

        // Select a picker but leave row empty
        await user.click(screen.getAllByRole('checkbox')[0]);

        const submitBtn = screen.getByRole('button', { name: /assign row/i });
        expect(submitBtn).toBeDisabled();
    });

    it('submit button is disabled when no pickers selected', async () => {
        const { user } = renderModal();

        // Fill row number but select no pickers
        await user.type(screen.getByPlaceholderText('12'), '5');

        const submitBtn = screen.getByRole('button', { name: /assign row 5/i });
        expect(submitBtn).toBeDisabled();
    });

    it('submit button is enabled when row + pickers are provided', async () => {
        const { user } = renderModal();

        await user.type(screen.getByPlaceholderText('12'), '7');
        await user.click(screen.getAllByRole('checkbox')[0]);

        const submitBtn = screen.getByRole('button', { name: /assign row 7/i });
        expect(submitBtn).toBeEnabled();
    });

    // ═══════════════════════════════════════
    // SUCCESSFUL ASSIGNMENT
    // ═══════════════════════════════════════

    it('calls onAssign with correct args and auto-closes on success', async () => {
        const { user } = renderModal();

        // Fill form
        await user.type(screen.getByPlaceholderText('12'), '14');

        // Change side to North
        await user.selectOptions(screen.getByLabelText('Side'), 'North');

        // Select two pickers
        await user.click(screen.getAllByRole('checkbox')[0]); // Alice (p1)
        await user.click(screen.getAllByRole('checkbox')[1]); // Bob (p2)

        // Submit
        await user.click(screen.getByRole('button', { name: /assign row 14/i }));

        // Verify onAssign was called with parsed row number, side, and selected picker IDs
        expect(mockOnAssign).toHaveBeenCalledOnce();
        expect(mockOnAssign).toHaveBeenCalledWith(14, 'North', ['p1', 'p2']);

        // Modal should auto-close after successful assignment
        expect(mockOnClose).toHaveBeenCalledOnce();
    });

    it('defaults side to South', () => {
        renderModal();

        const sideSelect = screen.getByLabelText('Side') as HTMLSelectElement;
        expect(sideSelect.value).toBe('South');
    });

    // ═══════════════════════════════════════
    // ERROR HANDLING
    // ═══════════════════════════════════════

    it('shows error toast when onAssign rejects', async () => {
        mockOnAssign.mockRejectedValueOnce(new Error('Network failure'));
        const { user } = renderModal();

        await user.type(screen.getByPlaceholderText('12'), '3');
        await user.click(screen.getAllByRole('checkbox')[0]);
        await user.click(screen.getByRole('button', { name: /assign row 3/i }));

        expect(mockShowToast).toHaveBeenCalledWith('Error assigning row', 'error');
        // Modal should NOT close on error
        expect(mockOnClose).not.toHaveBeenCalled();
    });

    // ═══════════════════════════════════════
    // EMPTY STATE
    // ═══════════════════════════════════════

    it('shows empty state when all pickers are off duty', () => {
        const offDutyOnly: PickerForAssignment[] = [
            { id: 'p1', name: 'Alice', avatar: 'A', idNumber: 'PK001', status: 'Off Duty' },
        ];

        renderModal(offDutyOnly);

        expect(screen.getByText('No active pickers available')).toBeInTheDocument();
    });
});
