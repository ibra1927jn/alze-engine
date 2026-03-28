/**
 * weeklyReportSections — HTML section builders for PDF export
 * Extracted from weeklyReportExport.ts to reduce file size.
 * 
 * Each function returns an HTML string fragment for a report section.
 * @module utils/weeklyReportSections
 */
import { PickerBreakdown } from '@/services/payroll.service';
import { TeamRanking } from '@/hooks/useWeeklyReport';

// Re-export chart builders from dedicated module
export { buildSparkline, buildChartSection, buildDistribution, buildCostAnalysis, buildDailySummary, buildDailySummaryTable } from './weeklyReportCharts';

interface CrewMember {
    id: string;
    picker_id: string;
    name: string;
    team_leader_id?: string;
}

// ── Resolve team name for a picker ──
export function getTeamName(p: PickerBreakdown, crew: CrewMember[]): string {
    const crewMember = crew.find(c => c.picker_id === p.picker_id || c.name === p.picker_name);
    const leaderId = crewMember?.team_leader_id || '';
    const leader = crew.find(c => c.id === leaderId);
    return leader?.name || 'Unassigned';
}



// ── Section builders (HTML strings) ──

export function buildKpiStrip(totalBuckets: number, totalTons: number, totalHours: number, totalEarnings: number, costPerBin: number, avgBPA: number, pickerCount: number, totalTopUp: number): string {
    return `<div style="margin: 12px 30px 0; border: 1px solid #e2e8f0; border-radius: 8px; overflow: hidden; background: white;">
    <table><tr>
        <td class="kpi-cell"><div class="kpi-label">Total Bins</div><div class="kpi-value" style="color: #059669;">${totalBuckets}</div></td>
        <td class="kpi-cell"><div class="kpi-label">Total Tons</div><div class="kpi-value" style="color: #1d4ed8;">${totalTons.toFixed(1)}</div></td>
        <td class="kpi-cell"><div class="kpi-label">Hours</div><div class="kpi-value" style="color: #b45309;">${totalHours.toFixed(0)}h</div></td>
        <td class="kpi-cell"><div class="kpi-label">Labour Cost</div><div class="kpi-value" style="color: #7e22ce;">$${totalEarnings.toFixed(0)}</div></td>
        <td class="kpi-cell"><div class="kpi-label">Cost / Bin</div><div class="kpi-value" style="color: #be123c;">$${costPerBin.toFixed(2)}</div></td>
        <td class="kpi-cell"><div class="kpi-label">Avg Bins/Hr</div><div class="kpi-value" style="color: #0d9488;">${avgBPA.toFixed(1)}</div></td>
        <td class="kpi-cell"><div class="kpi-label">Workforce</div><div class="kpi-value" style="color: #4338ca;">${pickerCount}</div></td>
        <td class="kpi-cell" style="background: ${totalTopUp > 0 ? '#fef2f2' : '#f0fdf4'};"><div class="kpi-label" style="color: ${totalTopUp > 0 ? '#991b1b' : '#166534'};">Top-Up Bleed</div><div class="kpi-value" style="color: ${totalTopUp > 0 ? '#dc2626' : '#059669'};">$${totalTopUp.toFixed(0)}</div></td>
    </tr></table>
</div>`;
}

export function buildInsightStrip(bestPicker: PickerBreakdown | undefined, totalTopUp: number, belowMinCount: number, aboveAvgCount: number, totalPickers: number): string {
    return `<div style="margin: 8px 30px 0; display: flex; gap: 8px; font-size: 10px;">
    <div style="flex: 1; background: #fffbeb; border: 1px solid #fde68a; border-radius: 6px; padding: 6px 10px; display: flex; align-items: center; gap: 6px;">
        <span style="font-size: 14px;">⭐</span>
        <span style="font-weight: 800; color: #92400e;">${bestPicker?.picker_name || 'N/A'}</span>
        <span style="color: #a16207; margin-left: auto;">${bestPicker ? bestPicker.buckets + ' bins • $' + bestPicker.total_earnings.toFixed(0) : '—'}</span>
    </div>
    <div style="flex: 1; background: ${totalTopUp > 0 ? '#fef2f2' : '#f0fdf4'}; border: 1px solid ${totalTopUp > 0 ? '#fecdd3' : '#bbf7d0'}; border-radius: 6px; padding: 6px 10px; display: flex; align-items: center; gap: 6px;">
        <span style="font-size: 14px;">${totalTopUp > 0 ? '💸' : '✅'}</span>
        <span style="font-weight: 800; color: ${totalTopUp > 0 ? '#dc2626' : '#059669'};">Wage Bleeding: $${totalTopUp.toFixed(2)}</span>
        <span style="color: ${totalTopUp > 0 ? '#b91c1c' : '#16a34a'}; margin-left: auto;">${belowMinCount} below min</span>
    </div>
    <div style="flex: 1; background: #eef2ff; border: 1px solid #c7d2fe; border-radius: 6px; padding: 6px 10px; display: flex; align-items: center; gap: 6px;">
        <span style="font-size: 14px;">📊</span>
        <span style="font-weight: 800; color: #4338ca;">${aboveAvgCount}/${totalPickers} above avg</span>
        <span style="color: #6366f1; margin-left: auto;">${totalPickers > 0 ? ((aboveAvgCount / totalPickers) * 100).toFixed(0) : 0}% on target</span>
    </div>
</div>`;
}



export function buildTeamRows(teamRankings: TeamRanking[], avgBPA: number): string {
    const maxTeamBuckets = Math.max(...teamRankings.map(t => t.buckets), 1);
    return teamRankings.map((t, i) => {
        const pct = (t.buckets / maxTeamBuckets) * 100;
        const medal = i === 0 ? '🥇' : i === 1 ? '🥈' : i === 2 ? '🥉' : `${i + 1}`;
        const barCol = i === 0 ? '#f59e0b' : i === 1 ? '#94a3b8' : i === 2 ? '#f97316' : '#6366f1';
        return `<tr style="border-bottom: 1px solid #f1f5f9; background: ${i % 2 === 0 ? '#fff' : '#fafbfc'};">
            <td style="padding: 5px 8px; text-align: center; font-size: 13px; width: 30px;">${medal}</td>
            <td style="padding: 5px 8px; font-weight: 700; font-size: 11px; color: #1e293b;">${t.name}</td>
            <td style="padding: 5px 8px; text-align: right; font-weight: 700; font-size: 11px; color: #0284c7;">${t.buckets}</td>
            <td style="padding: 5px 8px; width: 200px;">
                <div style="background: #f1f5f9; border-radius: 4px; height: 12px; overflow: hidden;">
                    <div style="width: ${pct}%; height: 100%; background: ${barCol}; border-radius: 4px;"></div>
                </div>
            </td>
            <td style="padding: 5px 8px; text-align: right; font-weight: 700; color: ${t.bpa >= avgBPA ? '#059669' : '#dc2626'}; font-size: 11px;">${t.bpa.toFixed(1)}/hr</td>
            <td style="padding: 5px 8px; text-align: right; font-weight: 800; font-size: 11px; color: #1e293b;">$${t.earnings.toFixed(0)}</td>
            <td style="padding: 5px 8px; text-align: right; font-size: 10px; color: #64748b;">${t.count} pickers</td>
        </tr>`;
    }).join('');
}

export function buildTeamSection(teamRowsHtml: string, teamCount: number): string {
    return `<div style="margin: 10px 30px 0;">
    <div class="section-title">🏆 Team Rankings <span style="font-weight: 400; color: #94a3b8; font-size: 9px; margin-left: 4px;">${teamCount} teams</span></div>
    <table style="border: 1px solid #e2e8f0; border-radius: 6px; overflow: hidden;">
        <thead><tr style="background: #f8fafc; border-bottom: 1px solid #e2e8f0;">
            <th style="padding: 4px 8px; font-size: 8px; text-transform: uppercase; color: #94a3b8; width: 30px;"></th>
            <th style="padding: 4px 8px; font-size: 8px; text-transform: uppercase; color: #94a3b8; text-align: left;">Team</th>
            <th style="padding: 4px 8px; font-size: 8px; text-transform: uppercase; color: #94a3b8; text-align: right;">Bins</th>
            <th style="padding: 4px 8px; font-size: 8px; text-transform: uppercase; color: #94a3b8; width: 200px;"></th>
            <th style="padding: 4px 8px; font-size: 8px; text-transform: uppercase; color: #94a3b8; text-align: right;">Rate</th>
            <th style="padding: 4px 8px; font-size: 8px; text-transform: uppercase; color: #94a3b8; text-align: right;">Earnings</th>
            <th style="padding: 4px 8px; font-size: 8px; text-transform: uppercase; color: #94a3b8; text-align: right;">Size</th>
        </tr></thead>
        <tbody>${teamRowsHtml || '<tr><td colspan="7" style="padding: 12px; text-align: center; color: #94a3b8;">No team data</td></tr>'}</tbody>
    </table>
</div>`;
}

export function buildPickerRows(sortedPickers: PickerBreakdown[], crew: CrewMember[], avgBPA: number): string {
    const minWage = 23.15;
    return sortedPickers.map((p, i) => {
        const bpa = p.hours_worked > 0 ? p.buckets / p.hours_worked : 0;
        const teamName = getTeamName(p, crew);
        const tier = bpa >= avgBPA * 1.1 ? '#059669' : bpa >= avgBPA * 0.8 ? '#f59e0b' : '#ef4444';
        const bg = i % 2 === 0 ? '#ffffff' : '#f8fafc';
        const topUp = p.top_up_required > 0 ? `<span style="color:#dc2626;font-weight:700;">$${p.top_up_required.toFixed(2)}</span>` : '<span style="color:#cbd5e1;">—</span>';
        const status = p.is_below_minimum ? '<span style="color:#dc2626;font-weight:700;">⚠ BELOW</span>' : '<span style="color:#059669;">✓</span>';
        const days = p.hours_worked > 0 ? Math.max(1, Math.round(p.hours_worked / 8)) : 0;
        const avgPerDay = days > 0 ? (p.buckets / days).toFixed(1) : '0';
        const dollarPerHr = p.hours_worked > 0 ? (p.total_earnings / p.hours_worked).toFixed(2) : '0';
        const wageColor = p.hours_worked > 0 && (p.total_earnings / p.hours_worked) < minWage ? '#dc2626' : '#059669';
        return `<tr style="border-bottom:1px solid #f1f5f9;background:${bg};">
            <td style="width:3px;background:${tier};padding:0;"></td>
            <td style="padding:4px 6px;text-align:center;font-size:10px;color:#94a3b8;font-weight:700;">${i + 1}</td>
            <td style="padding:4px 6px;text-align:center;font-family:monospace;font-size:10px;color:#6366f1;font-weight:700;">${p.picker_id}</td>
            <td style="padding:4px 6px;font-size:10px;font-weight:600;color:#1e293b;">${p.picker_name}</td>
            <td style="padding:4px 6px;font-size:9px;color:#64748b;">${teamName}</td>
            <td style="padding:4px 6px;text-align:center;font-size:10px;font-weight:700;color:#64748b;">${days}d</td>
            <td style="padding:4px 6px;text-align:right;font-size:11px;font-weight:800;color:#0284c7;">${p.buckets}</td>
            <td style="padding:4px 6px;text-align:right;font-size:10px;color:#475569;">${avgPerDay}</td>
            <td style="padding:4px 6px;text-align:right;font-size:10px;color:#64748b;">${p.hours_worked.toFixed(1)}h</td>
            <td style="padding:4px 6px;text-align:right;font-size:10px;font-weight:700;color:${tier};">${bpa.toFixed(1)}</td>
            <td style="padding:4px 6px;text-align:right;font-size:10px;color:${wageColor};font-weight:700;">$${dollarPerHr}</td>
            <td style="padding:4px 6px;text-align:right;font-size:10px;color:#475569;">$${p.piece_rate_earnings.toFixed(2)}</td>
            <td style="padding:4px 6px;text-align:right;font-size:10px;">${topUp}</td>
            <td style="padding:4px 6px;text-align:right;font-size:11px;font-weight:800;color:#1e293b;">$${p.total_earnings.toFixed(2)}</td>
            <td style="padding:4px 6px;text-align:center;font-size:9px;">${status}</td>
        </tr>`;
    }).join('');
}

export function buildPickerSection(pickerRows: string, pickerCount: number, belowMinCount: number, totalTopUp: number, totalDays: number, totalBuckets: number, avgBinsPerDay: string, totalHours: number, avgBPA: number, avgDollarPerHr: string, totalPieceRate: number, totalEarnings: number): string {
    const thStyle = 'padding: 5px 6px; font-size: 8px; text-transform: uppercase; color: #94a3b8;';
    return `<div style="margin: 10px 30px;">
    <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 4px;">
        <div class="section-title" style="border: none; padding-bottom: 0; margin-bottom: 0;">📋 Picker Performance Detail <span style="font-weight: 400; color: #94a3b8; font-size: 9px; margin-left: 4px;">${pickerCount} pickers</span></div>
        <div style="display: flex; gap: 8px; font-size: 9px;">
            <span style="color: #dc2626; font-weight: 700;">⚠ Below Min: ${belowMinCount}</span>
            <span style="color: #6366f1; font-weight: 700;">Top-Up: $${totalTopUp.toFixed(2)}</span>
        </div>
    </div>
    <table style="border: 1px solid #e2e8f0; border-radius: 6px; overflow: hidden; font-size: 10px;">
        <thead><tr style="background: #f8fafc; border-bottom: 2px solid #e2e8f0;">
            <th style="width: 3px;"></th>
            <th style="${thStyle} text-align: center; width: 24px;">#</th>
            <th style="${thStyle} text-align: center;">ID</th>
            <th style="${thStyle} text-align: left;">Name</th>
            <th style="${thStyle} text-align: left;">Team</th>
            <th style="${thStyle} text-align: center;">Days</th>
            <th style="${thStyle} text-align: right;">Bins</th>
            <th style="${thStyle} text-align: right;">Avg/Day</th>
            <th style="${thStyle} text-align: right;">Hours</th>
            <th style="${thStyle} text-align: right;">Bins/Hr</th>
            <th style="${thStyle} text-align: right;">$/Hr</th>
            <th style="${thStyle} text-align: right;">Piece $</th>
            <th style="${thStyle} text-align: right;">Top-Up</th>
            <th style="${thStyle} text-align: right;">Total $</th>
            <th style="${thStyle} text-align: center;">Status</th>
        </tr></thead>
        <tbody>${pickerRows || '<tr><td colspan="15" style="padding: 20px; text-align: center; color: #94a3b8;">No picker data</td></tr>'}</tbody>
        <tfoot><tr style="background: #0f172a; color: white; font-weight: 700;">
            <td style="width: 3px; background: #6366f1;"></td>
            <td colspan="4" style="padding: 6px 8px; font-size: 10px;">TOTALS — ${pickerCount} pickers</td>
            <td style="padding: 6px; text-align: center; font-size: 10px;">${totalDays}d</td>
            <td style="padding: 6px; text-align: right; font-size: 10px; font-weight: 800;">${totalBuckets}</td>
            <td style="padding: 6px; text-align: right; font-size: 10px;">${avgBinsPerDay}</td>
            <td style="padding: 6px; text-align: right; font-size: 10px;">${totalHours.toFixed(1)}h</td>
            <td style="padding: 6px; text-align: right; font-size: 10px; font-weight: 800;">${avgBPA.toFixed(1)}</td>
            <td style="padding: 6px; text-align: right; font-size: 10px;">$${avgDollarPerHr}</td>
            <td style="padding: 6px; text-align: right; font-size: 10px;">$${totalPieceRate.toFixed(2)}</td>
            <td style="padding: 6px; text-align: right; font-size: 10px; color: #fca5a5;">$${totalTopUp.toFixed(2)}</td>
            <td style="padding: 6px; text-align: right; font-size: 11px; font-weight: 900;">$${totalEarnings.toFixed(2)}</td>
            <td style="padding: 6px; text-align: center; font-size: 9px;">${belowMinCount} ⚠</td>
        </tr></tfoot>
    </table>
</div>`;
}



export function buildTeamBreakdown(teamRankings: TeamRanking[], sortedPickers: PickerBreakdown[], crew: CrewMember[], avgBPA: number): string {
    return teamRankings.map((team, ti) => {
        const teamPickers = sortedPickers.filter(p => getTeamName(p, crew) === team.name);
        if (teamPickers.length === 0) return '';
        const tBins = teamPickers.reduce((s, p) => s + p.buckets, 0);
        const tHrs = teamPickers.reduce((s, p) => s + p.hours_worked, 0);
        const tErn = teamPickers.reduce((s, p) => s + p.total_earnings, 0);
        const tTopUp = teamPickers.reduce((s, p) => s + p.top_up_required, 0);
        const tBelowMin = teamPickers.filter(p => p.is_below_minimum).length;
        const tAvgBpa = tHrs > 0 ? tBins / tHrs : 0;
        const rows = teamPickers.map((p, i) => {
            const bpa = p.hours_worked > 0 ? p.buckets / p.hours_worked : 0;
            const days = p.hours_worked > 0 ? Math.max(1, Math.round(p.hours_worked / 8)) : 0;
            const avgPd = days > 0 ? (p.buckets / days).toFixed(1) : '0';
            const dph = p.hours_worked > 0 ? (p.total_earnings / p.hours_worked).toFixed(2) : '0';
            const tier = bpa >= avgBPA * 1.1 ? '#059669' : bpa >= avgBPA * 0.8 ? '#f59e0b' : '#ef4444';
            const bg = i % 2 === 0 ? '#fff' : '#f8fafc';
            return `<tr style="border-bottom:1px solid #f1f5f9;background:${bg};">
                <td style="width:3px;background:${tier};padding:0;"></td>
                <td style="padding:4px 6px;text-align:center;font-size:10px;color:#94a3b8;font-weight:700;">${i + 1}</td>
                <td style="padding:4px 6px;font-family:monospace;font-size:10px;color:#6366f1;font-weight:700;">${p.picker_id}</td>
                <td style="padding:4px 6px;font-size:10px;font-weight:600;color:#1e293b;">${p.picker_name}</td>
                <td style="padding:4px 6px;text-align:center;font-size:10px;color:#64748b;">${days}d</td>
                <td style="padding:4px 6px;text-align:right;font-size:11px;font-weight:800;color:#0284c7;">${p.buckets}</td>
                <td style="padding:4px 6px;text-align:right;font-size:10px;color:#475569;">${avgPd}</td>
                <td style="padding:4px 6px;text-align:right;font-size:10px;color:#64748b;">${p.hours_worked.toFixed(1)}h</td>
                <td style="padding:4px 6px;text-align:right;font-size:10px;font-weight:700;color:${tier};">${bpa.toFixed(1)}</td>
                <td style="padding:4px 6px;text-align:right;font-size:10px;color:#475569;">$${dph}/hr</td>
                <td style="padding:4px 6px;text-align:right;font-size:11px;font-weight:800;color:#1e293b;">$${p.total_earnings.toFixed(2)}</td>
                <td style="padding:4px 6px;text-align:center;font-size:9px;">${p.is_below_minimum ? '<span style="color:#dc2626;font-weight:700;">⚠</span>' : '<span style="color:#059669;">✓</span>'}</td>
            </tr>`;
        }).join('');
        const thStyle = 'padding:4px 6px;font-size:8px;text-transform:uppercase;color:#94a3b8;';
        const medalColor = ti === 0 ? '#f59e0b' : ti === 1 ? '#94a3b8' : '#6366f1';
        return `<div style="page-break-before: always; margin: 0 30px; padding-top: 16px;">
            <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:6px;">
                <div style="display:flex;align-items:center;gap:8px;">
                    <span style="font-size:16px;">${ti === 0 ? '🥇' : ti === 1 ? '🥈' : ti === 2 ? '🥉' : '👤'}</span>
                    <span style="font-size:14px;font-weight:800;color:#1e293b;">${team.name}'s Team</span>
                    <span style="font-size:10px;color:#94a3b8;">${teamPickers.length} pickers</span>
                </div>
                <div style="display:flex;gap:8px;font-size:9px;">
                    <span style="background:#e0f2fe;color:#0369a1;padding:2px 8px;border-radius:4px;font-weight:700;">${tBins} bins</span>
                    <span style="background:${tAvgBpa >= avgBPA ? '#dcfce7' : '#fef2f2'};color:${tAvgBpa >= avgBPA ? '#059669' : '#dc2626'};padding:2px 8px;border-radius:4px;font-weight:700;">${tAvgBpa.toFixed(1)} bins/hr</span>
                    <span style="font-weight:800;color:#1e293b;">$${tErn.toFixed(0)}</span>
                    ${tTopUp > 0 ? `<span style="color:#dc2626;font-weight:700;">Top-Up: $${tTopUp.toFixed(2)}</span>` : ''}
                    ${tBelowMin > 0 ? `<span style="color:#dc2626;">⚠ ${tBelowMin} below min</span>` : ''}
                </div>
            </div>
            <table style="width:100%;border:1px solid #e2e8f0;border-radius:6px;overflow:hidden;border-collapse:collapse;font-size:10px;">
                <thead><tr style="background:#f8fafc;border-bottom:2px solid #e2e8f0;">
                    <th style="width:3px;"></th>
                    <th style="${thStyle}">#</th><th style="${thStyle}">ID</th>
                    <th style="${thStyle}text-align:left;">Name</th><th style="${thStyle}">Days</th>
                    <th style="${thStyle}text-align:right;">Bins</th><th style="${thStyle}text-align:right;">Avg/Day</th>
                    <th style="${thStyle}text-align:right;">Hours</th><th style="${thStyle}text-align:right;">Bins/Hr</th>
                    <th style="${thStyle}text-align:right;">$/Hr</th><th style="${thStyle}text-align:right;">Total $</th>
                    <th style="${thStyle}">Status</th>
                </tr></thead>
                <tbody>${rows}</tbody>
                <tfoot><tr style="background:#0f172a;color:white;font-weight:700;">
                    <td style="width:3px;background:${medalColor};"></td>
                    <td colspan="4" style="padding:5px 6px;font-size:10px;">Team Total</td>
                    <td style="padding:5px 6px;text-align:right;font-size:10px;font-weight:800;">${tBins}</td>
                    <td></td>
                    <td style="padding:5px 6px;text-align:right;font-size:10px;">${tHrs.toFixed(1)}h</td>
                    <td style="padding:5px 6px;text-align:right;font-size:10px;font-weight:800;">${tAvgBpa.toFixed(1)}</td>
                    <td></td>
                    <td style="padding:5px 6px;text-align:right;font-size:10px;font-weight:800;">$${tErn.toFixed(2)}</td>
                    <td style="padding:5px 6px;text-align:center;font-size:9px;">${tBelowMin > 0 ? tBelowMin + ' ⚠' : '✓'}</td>
                </tr></tfoot>
            </table>
        </div>`;
    }).join('');
}
