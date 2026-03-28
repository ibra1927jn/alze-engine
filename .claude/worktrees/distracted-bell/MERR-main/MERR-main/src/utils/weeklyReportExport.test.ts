import { describe, it, expect, vi } from 'vitest';
import { exportCSV } from './weeklyReportExport';

// Minimal mock data to test the export flow
const mockPickers = [
    {
        picker_id: 'P-001',
        picker_name: 'John Smith',
        buckets: 100,
        hours_worked: 40,
        piece_rate_earnings: 500,
        hourly_rate: 12.5,
        minimum_required: 926,
        top_up_required: 426,
        total_earnings: 926,
        is_below_minimum: true,
    },
    {
        picker_id: 'P-002',
        picker_name: 'Jane Doe',
        buckets: 200,
        hours_worked: 40,
        piece_rate_earnings: 1000,
        hourly_rate: 25,
        minimum_required: 926,
        top_up_required: 0,
        total_earnings: 1000,
        is_below_minimum: false,
    },
];

const mockCrew = [
    { id: 'crew-1', picker_id: 'P-001', name: 'John Smith', team_leader_id: 'leader-1' },
    { id: 'crew-2', picker_id: 'P-002', name: 'Jane Doe', team_leader_id: 'leader-1' },
    { id: 'leader-1', picker_id: 'L-001', name: 'Team Leader A' },
];

const mockContext = {
    pickers: mockPickers,
    binsTrend: [{ label: 'Mon', value: 50 }, { label: 'Tue', value: 80 }],
    workforceTrend: [{ label: 'Mon', value: 5 }, { label: 'Tue', value: 8 }],
    teamRankings: [],
    crew: mockCrew,
    orchardName: 'Test Orchard',
    totalBuckets: 300,
    totalHours: 80,
    totalEarnings: 1926,
    avgBPA: 3.75,
    costPerBin: 6.42,
    exportSections: { summary: true, charts: true, teams: true, pickerDetail: true },
};

describe('weeklyReportExport — CSV export', () => {
    it('exportCSV creates a downloadable link and triggers click', () => {
        const mockAnchor = { click: vi.fn(), href: '', download: '' };
        vi.spyOn(document, 'createElement').mockReturnValue(mockAnchor as unknown as HTMLElement);
        vi.spyOn(URL, 'createObjectURL').mockReturnValue('blob:test-url');
        vi.spyOn(URL, 'revokeObjectURL').mockImplementation(() => { });

        exportCSV(mockContext);

        expect(document.createElement).toHaveBeenCalledWith('a');
        expect(mockAnchor.click).toHaveBeenCalledOnce();
        expect(mockAnchor.download).toContain('harvest_report_');
        expect(mockAnchor.download).toContain('.csv');
        expect(URL.revokeObjectURL).toHaveBeenCalledWith('blob:test-url');
    });

    it('exportCSV does not throw with empty pickers', () => {
        const emptyCtx = { ...mockContext, pickers: [] };
        const mockAnchor = { click: vi.fn(), href: '', download: '' };
        vi.spyOn(document, 'createElement').mockReturnValue(mockAnchor as unknown as HTMLElement);
        vi.spyOn(URL, 'createObjectURL').mockReturnValue('blob:test');
        vi.spyOn(URL, 'revokeObjectURL').mockImplementation(() => { });

        expect(() => exportCSV(emptyCtx)).not.toThrow();
    });
});
