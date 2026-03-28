/**
 * Timesheet Editor
 *
 * Admin-only view for correcting past attendance records.
 * Shows daily attendance with inline editing of check-in/out times.
 * All corrections require a reason and create an audit trail.
 */

import { logger } from '@/utils/logger';
import { useToast } from '@/hooks/useToast';
import Toast from '@/components/ui/Toast';
import React, { useState, useEffect, useCallback } from 'react';

import { attendanceService } from '@/services/attendance.service';
import { todayNZST } from '@/utils/nzst';
import { useAuth } from '@/context/AuthContext';
import { AttendanceRow, formatTimeForInput, formatTime, calculateHours, isAbnormal } from './timesheet-utils';

interface TimesheetEditorProps {
    orchardId: string;
}

export default function TimesheetEditor({ orchardId }: TimesheetEditorProps) {
    const { appUser } = useAuth();
    const [selectedDate, setSelectedDate] = useState(todayNZST());
    const [records, setRecords] = useState<AttendanceRow[]>([]);
    const [loading, setLoading] = useState(false);
    const [editingId, setEditingId] = useState<string | null>(null);
    const [editValues, setEditValues] = useState({ checkIn: '', checkOut: '', reason: '' });
    const [saving, setSaving] = useState(false);
    const [successMsg, setSuccessMsg] = useState('');
    const { toast, showToast, hideToast } = useToast();
    // ========================================
    // DATA LOADING
    // ========================================

    const loadRecords = useCallback(async () => {
        if (!orchardId) return;
        setLoading(true);
        try {
            const data = await attendanceService.getAttendanceByDate(orchardId, selectedDate);
            setRecords(data as AttendanceRow[]);
        } catch (err) {
            logger.error('Failed to load attendance:', err);
        } finally {
            setLoading(false);
        }
    }, [orchardId, selectedDate]);

    useEffect(() => {
        loadRecords();
    }, [loadRecords]);

    // ========================================
    // EDITING
    // ========================================

    const startEdit = (record: AttendanceRow) => {
        setEditingId(record.id);
        setEditValues({
            checkIn: record.check_in_time ? formatTimeForInput(record.check_in_time) : '',
            checkOut: record.check_out_time ? formatTimeForInput(record.check_out_time) : '',
            reason: '',
        });
    };

    const cancelEdit = () => {
        setEditingId(null);
        setEditValues({ checkIn: '', checkOut: '', reason: '' });
    };

    const saveCorrection = async (record: AttendanceRow) => {
        if (!editValues.reason.trim()) {
            showToast('A correction reason is required for audit compliance.', 'warning');
            return;
        }

        if (!appUser?.id) return;

        setSaving(true);
        try {
            const updates: { check_in_time?: string; check_out_time?: string } = {};

            if (editValues.checkIn) {
                updates.check_in_time = `${record.date}T${editValues.checkIn}:00+13:00`;
            }
            if (editValues.checkOut) {
                updates.check_out_time = `${record.date}T${editValues.checkOut}:00+13:00`;
            }

            await attendanceService.correctAttendance(
                record.id,
                updates,
                editValues.reason.trim(),
                appUser.id
            );

            setEditingId(null);
            setSuccessMsg(`Corrected ${record.picker?.name || 'picker'}'s timesheet`);
            setTimeout(() => setSuccessMsg(''), 3000);
            await loadRecords();
        } catch (err) {
            showToast(`Error: ${err instanceof Error ? err.message : 'Unknown'}`, 'error');
        } finally {
            setSaving(false);
        }
    };



    return (
        <div className="space-y-4">
            {/* Header */}
            <div className="flex items-center justify-between">
                <div className="flex items-center gap-3">
                    <div className="w-10 h-10 bg-amber-100 rounded-xl flex items-center justify-center">
                        <span className="material-symbols-outlined text-xl text-amber-600">schedule</span>
                    </div>
                    <div>
                        <h2 className="text-lg font-bold text-text-main">Timesheet Editor</h2>
                        <p className="text-sm text-text-muted">Correct attendance records with audit trail</p>
                    </div>
                </div>

                {/* Date Picker */}
                <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-base text-text-muted">calendar_today</span>
                    <input
                        type="date"
                        value={selectedDate}
                        onChange={(e) => setSelectedDate(e.target.value)}
                        max={todayNZST()}
                        title="Select date"
                        className="px-3 py-2 border border-border-light rounded-xl text-sm font-medium text-text-sub bg-white focus:ring-2 focus:ring-amber-500 outline-none"
                    />
                </div>
            </div>

            {/* Success Banner */}
            {successMsg && (
                <div className="flex items-center gap-2 bg-green-50 border border-green-200 rounded-xl px-4 py-3">
                    <span className="material-symbols-outlined text-lg text-green-600">check_circle</span>
                    <span className="text-sm font-medium text-green-700">{successMsg}</span>
                </div>
            )}

            {/* Loading */}
            {loading && (
                <div className="flex justify-center py-12">
                    <span className="material-symbols-outlined text-3xl text-text-muted animate-spin">progress_activity</span>
                </div>
            )}

            {/* Empty State */}
            {!loading && records.length === 0 && (
                <div className="text-center py-12 bg-white rounded-xl border border-border-light">
                    <span className="material-symbols-outlined text-5xl mx-auto text-slate-300 mb-4">schedule</span>
                    <p className="text-text-muted font-medium">No attendance records for {selectedDate}</p>
                    <p className="text-sm text-text-muted mt-1">Select a different date to view records</p>
                </div>
            )}

            {/* Table */}
            {!loading && records.length > 0 && (
                <div className="bg-white rounded-xl border border-border-light overflow-hidden">
                    <table className="w-full text-sm">
                        <thead className="bg-slate-50">
                            <tr>
                                <th className="text-left px-4 py-3 text-text-sub font-semibold">Picker</th>
                                <th className="text-left px-4 py-3 text-text-sub font-semibold">Check-In</th>
                                <th className="text-left px-4 py-3 text-text-sub font-semibold">Check-Out</th>
                                <th className="text-left px-4 py-3 text-text-sub font-semibold">Hours</th>
                                <th className="text-left px-4 py-3 text-text-sub font-semibold">Status</th>
                                <th className="text-right px-4 py-3 text-text-sub font-semibold">Action</th>
                            </tr>
                        </thead>
                        <tbody>
                            {records.map((record) => {
                                const hours = calculateHours(record.check_in_time, record.check_out_time);
                                const abnormal = isAbnormal(hours);
                                const isEditing = editingId === record.id;

                                return (
                                    <tr key={record.id} className={`border-t border-border-light ${abnormal ? 'bg-red-50' : ''} ${isEditing ? 'bg-amber-50' : ''}`}>
                                        {/* Picker Name */}
                                        <td className="px-4 py-3">
                                            <div className="font-medium text-text-main">{record.picker?.name || 'Unknown'}</div>
                                            <div className="text-xs text-text-muted">{record.picker?.picker_id}</div>
                                        </td>

                                        {/* Check-In */}
                                        <td className="px-4 py-3">
                                            {isEditing ? (
                                                <input
                                                    type="time"
                                                    value={editValues.checkIn}
                                                    onChange={(e) => setEditValues(v => ({ ...v, checkIn: e.target.value }))}
                                                    title="Check-in time"
                                                    className="px-2 py-1 border border-amber-300 rounded-lg text-sm w-24 focus:ring-2 focus:ring-amber-500 outline-none"
                                                />
                                            ) : (
                                                <span className="font-mono">{formatTime(record.check_in_time)}</span>
                                            )}
                                        </td>

                                        {/* Check-Out */}
                                        <td className="px-4 py-3">
                                            {isEditing ? (
                                                <input
                                                    type="time"
                                                    value={editValues.checkOut}
                                                    onChange={(e) => setEditValues(v => ({ ...v, checkOut: e.target.value }))}
                                                    title="Check-out time"
                                                    className="px-2 py-1 border border-amber-300 rounded-lg text-sm w-24 focus:ring-2 focus:ring-amber-500 outline-none"
                                                />
                                            ) : (
                                                <span className={`font-mono ${!record.check_out_time ? 'text-red-500' : ''}`}>
                                                    {!record.check_out_time && <span className="material-symbols-outlined text-sm inline mr-1 text-red-500">warning</span>}
                                                    {formatTime(record.check_out_time)}
                                                </span>
                                            )}
                                        </td>

                                        {/* Hours */}
                                        <td className="px-4 py-3">
                                            <span className={`font-mono font-bold ${abnormal ? 'text-red-600' : 'text-text-sub'}`}>
                                                {hours !== null ? `${hours}h` : '—'}
                                            </span>
                                            {abnormal && (
                                                <span className="material-symbols-outlined text-sm inline ml-1 text-red-500">warning</span>
                                            )}
                                        </td>

                                        {/* Status */}
                                        <td className="px-4 py-3">
                                            {record.corrected_at ? (
                                                <span className="inline-flex items-center gap-1 px-2 py-0.5 bg-amber-100 text-amber-700 rounded-full text-xs font-medium">
                                                    <span className="material-symbols-outlined text-xs">edit</span>
                                                    Corrected
                                                </span>
                                            ) : (
                                                <span className="text-xs text-text-muted">{record.status}</span>
                                            )}
                                        </td>

                                        {/* Actions */}
                                        <td className="px-4 py-3 text-right">
                                            {isEditing ? (
                                                <div className="space-y-2">
                                                    <input
                                                        type="text"
                                                        placeholder="Reason for correction *"
                                                        value={editValues.reason}
                                                        onChange={(e) => setEditValues(v => ({ ...v, reason: e.target.value }))}
                                                        className="w-full px-2 py-1 border border-amber-300 rounded-lg text-sm focus:ring-2 focus:ring-amber-500 outline-none"
                                                    />
                                                    <div className="flex gap-1 justify-end">
                                                        <button
                                                            onClick={cancelEdit}
                                                            className="p-1.5 text-text-muted hover:text-text-sub rounded-lg"
                                                            aria-label="Cancel edit"
                                                            disabled={saving}
                                                        >
                                                            <span className="material-symbols-outlined text-base">close</span>
                                                        </button>
                                                        <button
                                                            onClick={() => saveCorrection(record)}
                                                            disabled={saving || !editValues.reason.trim()}
                                                            className="p-1.5 text-green-600 hover:text-green-700 rounded-lg disabled:opacity-50"
                                                            aria-label="Save correction"
                                                        >
                                                            {saving ? <span className="material-symbols-outlined text-base animate-spin">progress_activity</span> : <span className="material-symbols-outlined text-base">save</span>}
                                                        </button>
                                                    </div>
                                                </div>
                                            ) : (
                                                <button
                                                    onClick={() => startEdit(record)}
                                                    className="p-2 text-text-muted hover:text-amber-600 hover:bg-amber-50 rounded-lg transition-colors"
                                                    aria-label={`Edit ${record.picker?.name}'s timesheet`}
                                                >
                                                    <span className="material-symbols-outlined text-base">edit</span>
                                                </button>
                                            )}
                                        </td>
                                    </tr>
                                );
                            })}
                        </tbody>
                    </table>
                </div>
            )}

            {/* Info Box */}
            <div className="bg-amber-50 border border-amber-200 rounded-xl p-4">
                <div className="flex items-start gap-3">
                    <span className="material-symbols-outlined text-lg text-amber-600 mt-0.5">warning</span>
                    <div>
                        <p className="text-sm font-medium text-amber-800">Audit Compliance</p>
                        <p className="text-xs text-amber-700 mt-1">
                            All corrections are logged with your name, timestamp, and reason.
                            This data is immutable and visible in the Audit Log.
                        </p>
                    </div>
                </div>
            </div>
        </div>
    );
}
