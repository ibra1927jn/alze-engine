/**
 * StatsTab — QC Analytics
 * Extracted from QualityControl.tsx monolith
 */
import React from 'react';
import EmptyState from '@/components/ui/EmptyState';
import { GradeDistribution } from '@/services/qc.service';
import DistributionBar from './DistributionBar';

const GRADE_COLORS: Record<string, string> = {
    A: 'text-green-700',
    B: 'text-blue-700',
    C: 'text-amber-700',
    reject: 'text-red-700',
};

interface StatsTabProps {
    distribution: GradeDistribution;
}

export default function StatsTab({ distribution }: StatsTabProps) {
    if (distribution.total === 0) {
        return (
            <EmptyState
                icon="bar_chart"
                title="Grade distribution analytics"
                subtitle="Will show trends once inspections are logged"
                compact
            />
        );
    }

    const pct = (grade: keyof Omit<GradeDistribution, 'total'>) =>
        Math.round((distribution[grade] / distribution.total) * 100);

    return (
        <div className="space-y-4">
            <div className="bg-white rounded-lg border border-border-light shadow-sm p-5">
                <h3 className="text-sm font-semibold text-text-primary mb-4">Grade Distribution</h3>
                <DistributionBar distribution={distribution} large />
                <div className="grid grid-cols-4 gap-3 mt-4">
                    {(['A', 'B', 'C', 'reject'] as const).map(grade => (
                        <div key={grade} className="text-center">
                            <div className={`text-2xl font-bold ${GRADE_COLORS[grade]}`}>
                                {distribution[grade]}
                            </div>
                            <div className="text-xs text-text-secondary">
                                {grade === 'reject' ? 'Reject' : `Grade ${grade}`}
                            </div>
                            <div className="text-xs text-text-muted">
                                {pct(grade)}%
                            </div>
                        </div>
                    ))}
                </div>
            </div>

            <div className="bg-white rounded-lg border border-border-light shadow-sm p-5">
                <h3 className="text-sm font-semibold text-text-primary mb-2">Summary</h3>
                <div className="space-y-2 text-sm">
                    <div className="flex justify-between">
                        <span className="text-text-secondary">Total Inspections</span>
                        <span className="font-medium text-text-primary">{distribution.total}</span>
                    </div>
                    <div className="flex justify-between">
                        <span className="text-text-secondary">Export Quality (A)</span>
                        <span className="font-medium text-green-600">{pct('A')}%</span>
                    </div>
                    <div className="flex justify-between">
                        <span className="text-text-secondary">Rejection Rate</span>
                        <span className="font-medium text-red-600">{pct('reject')}%</span>
                    </div>
                </div>
            </div>
        </div>
    );
}
