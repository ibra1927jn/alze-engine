/**
 * SettingsView — Orchard Settings (Manager)
 *
 * Refactored architecture:
 *   SettingsView.tsx           — Thin orchestrator (~200 lines, UI only)
 *   useSettings.ts             — Data hook (state, handlers, save logic)
 *   settings/
 *     └── SettingsFormComponents.tsx — Reusable: SettingsSection, FormField, ReadonlyField, ToggleRow
 */
import React from 'react';
import { useSettings } from '@/hooks/useSettings';
import { useTranslation, SUPPORTED_LOCALES, type Locale } from '@/i18n';
import { DayClosureButton } from './DayClosureButton';
import PageHeader from '@/components/ui/PageHeader';
import { SettingsSection, FormField, ReadonlyField, ToggleRow } from './settings/SettingsFormComponents';

const SettingsView: React.FC = () => {
    const s = useSettings();
    const { locale, setLocale, t } = useTranslation();

    return (
        <div className="p-4 md:p-6 max-w-2xl mx-auto space-y-5 pb-24 animate-fade-in">
            <PageHeader icon="settings" title="Settings" subtitle={`${s.orchard?.name || 'Orchard'} configuration`} />

            {/* ── Profile Card ──────────────────────────── */}
            <section className="glass-card overflow-hidden section-enter stagger-1">
                <div className="relative">
                    <div className="h-20 gradient-primary opacity-90" />
                    <div className="px-5 pb-4 -mt-8">
                        <div className="flex items-end gap-4">
                            <div className="w-16 h-16 rounded-2xl bg-white shadow-lg border-4 border-white flex items-center justify-center text-xl font-black text-indigo-600">
                                {s.initials}
                            </div>
                            <div className="flex-1 min-w-0 pb-0.5">
                                <h3 className="text-base font-bold text-text-main truncate">{s.currentUser?.name || 'Manager'}</h3>
                                <p className="text-xs text-text-muted capitalize">{s.currentUser?.role || 'manager'} • {s.orchard?.name || 'No Orchard'}</p>
                            </div>
                        </div>
                        <div className="grid grid-cols-3 gap-3 mt-4">
                            {[
                                { value: s.orchard?.total_rows || '—', label: 'Rows' },
                                { value: `$${s.formData.piece_rate}`, label: 'Rate' },
                                { value: `${s.formData.target_tons}t`, label: 'Target' },
                            ].map(stat => (
                                <div key={stat.label} className="text-center p-2 rounded-xl bg-slate-50">
                                    <p className="text-lg font-bold text-text-main tabular-nums">{stat.value}</p>
                                    <p className="text-[10px] text-text-muted font-semibold uppercase tracking-wider">{stat.label}</p>
                                </div>
                            ))}
                        </div>
                    </div>
                </div>
            </section>

            {/* ── Harvest Configuration ────────────────── */}
            <SettingsSection icon="tune" iconBg="bg-indigo-50" iconColor="text-indigo-600" title="Harvest Configuration" subtitle="Rates & targets" accentColor="border-l-indigo-500" stagger={2}>
                <FormField label="Piece Rate (per bucket)" value={s.formData.piece_rate} onChange={(v) => s.handleChange('piece_rate', v)} prefix="$" type="number" step="0.50" />
                <FormField label="Minimum Wage (per hour)" value={s.formData.min_wage_rate} onChange={(v) => s.handleChange('min_wage_rate', v)} prefix="$" suffix="NZD" type="number" step="0.05" />
                <FormField label="Target Buckets / Hour" value={s.formData.min_buckets_per_hour} onChange={(v) => s.handleChange('min_buckets_per_hour', v)} type="number" step="1" />
                <FormField label="Daily Target (tons)" value={s.formData.target_tons} onChange={(v) => s.handleChange('target_tons', v)} type="number" step="1" />
            </SettingsSection>

            {/* ── Orchard Details ──────────────────────── */}
            <SettingsSection icon="park" iconBg="bg-emerald-50" iconColor="text-emerald-600" title="Orchard Details" subtitle="Farm information" accentColor="border-l-emerald-500" stagger={3}>
                <ReadonlyField label="Orchard Name" value={s.orchard?.name || '—'} icon="location_on" />
                <ReadonlyField label="Total Rows" value={String(s.orchard?.total_rows ?? '—')} icon="grid_view" />
                <FormField label="Fruit Variety" value={s.formData.variety} onChange={(v) => s.handleChange('variety', v)} type="select" options={['Cherry', 'Apple', 'Kiwifruit', 'Pear', 'Mix']} />
            </SettingsSection>

            {/* ── Compliance ──────────────────────────── */}
            <SettingsSection icon="verified_user" iconBg="bg-green-50" iconColor="text-green-600" title="Compliance Settings" subtitle="NZ labour regulations" accentColor="border-l-green-500" stagger={4}>
                <ToggleRow label="NZ Employment Standards" description="Enforce minimum wage and break requirements" checked={s.compliance.nz_employment_standards} onChange={(v) => s.setCompliance(prev => ({ ...prev, nz_employment_standards: v }))} icon="gavel" />
                <ToggleRow label="Automatic Wage Alerts" description="Notify when workers fall below minimum wage" checked={s.compliance.auto_wage_alerts} onChange={(v) => s.setCompliance(prev => ({ ...prev, auto_wage_alerts: v }))} icon="notification_important" />
                <ToggleRow label="Safety Verification Required" description="Require daily safety check before scanning" checked={s.compliance.safety_verification} onChange={(v) => s.setCompliance(prev => ({ ...prev, safety_verification: v }))} icon="health_and_safety" />
                <ToggleRow label="Audit Trail Logging" description="All actions are logged — cannot be disabled" checked={s.compliance.audit_trail} onChange={() => {/* locked */ }} locked icon="lock" />
            </SettingsSection>

            {/* ── Notifications ────────────────────────── */}
            <SettingsSection icon="notifications_active" iconBg="bg-amber-50" iconColor="text-amber-600" title="Notifications" subtitle="Push alerts & monitoring" accentColor="border-l-amber-500" stagger={5}>
                <ToggleRow label="Enable Push Notifications" description="Receive browser alerts for critical events" checked={s.notifEnabled} onChange={s.handleNotifToggle} icon="notifications" />
                {s.notifEnabled && (
                    <>
                        <div className="border-t border-slate-100 pt-3 mt-1">
                            <p className="text-[10px] text-text-muted font-bold uppercase tracking-widest mb-3">Alert Types</p>
                            <div className="space-y-3">
                                <ToggleRow label="Visa Expiry (7-day warning)" checked={s.notifTypes.visa_expiry} onChange={(v) => s.handleNotifType('visa_expiry', v)} icon="badge" compact />
                                <ToggleRow label="QC Reject Rate (>15%)" checked={s.notifTypes.qc_reject} onChange={(v) => s.handleNotifType('qc_reject', v)} icon="error" compact />
                                <ToggleRow label="Transport Pending (>30 min)" checked={s.notifTypes.transport} onChange={(v) => s.handleNotifType('transport', v)} icon="local_shipping" compact />
                                <ToggleRow label="Attendance Alerts" checked={s.notifTypes.attendance} onChange={(v) => s.handleNotifType('attendance', v)} icon="assignment_late" compact />
                            </div>
                        </div>
                        <div className="pt-2">
                            <button
                                onClick={s.handleSendTest}
                                className={`px-4 py-2.5 text-sm font-semibold rounded-xl transition-all flex items-center gap-2 ${s.notifTestSent ? 'text-green-700 bg-green-50 border border-green-200' : 'text-amber-700 bg-amber-50 border border-amber-200 hover:bg-amber-100 hover:border-amber-300 active:scale-[0.97]'}`}
                            >
                                <span className="material-symbols-outlined text-base">{s.notifTestSent ? 'check_circle' : 'send'}</span>
                                {s.notifTestSent ? 'Test Sent ✓' : 'Send Test Notification'}
                            </button>
                        </div>
                    </>
                )}
            </SettingsSection>

            {/* ── Language ──────────────────────────────── */}
            <SettingsSection icon="translate" iconBg="bg-violet-50" iconColor="text-violet-600" title={t('settings.language')} subtitle={t('settings.language_desc')} accentColor="border-l-violet-500" stagger={6}>
                <div className="grid grid-cols-3 gap-2">
                    {SUPPORTED_LOCALES.map((loc) => (
                        <button
                            key={loc.code}
                            onClick={() => setLocale(loc.code as Locale)}
                            className={`flex flex-col items-center gap-1.5 p-3 rounded-xl border-2 transition-all active:scale-[0.96] ${locale === loc.code ? 'border-violet-500 bg-violet-50 shadow-sm shadow-violet-100' : 'border-slate-100 hover:border-violet-200 hover:bg-slate-50'}`}
                        >
                            <span className="text-2xl">{loc.flag}</span>
                            <span className={`text-xs font-semibold ${locale === loc.code ? 'text-violet-700' : 'text-text-sub'}`}>{loc.nativeName}</span>
                            {locale === loc.code && (
                                <span className="text-[10px] text-violet-500 font-bold flex items-center gap-0.5">
                                    <span className="material-symbols-outlined text-xs">check_circle</span>Active
                                </span>
                            )}
                        </button>
                    ))}
                </div>
            </SettingsSection>

            {/* ── Save Button ──────────────────────────── */}
            <div className="space-y-3 section-enter stagger-7">
                <button
                    onClick={s.handleSave}
                    disabled={s.isSaving || !s.hasChanges}
                    className={`w-full flex items-center justify-center gap-2 px-4 py-3.5 rounded-xl font-bold text-sm transition-all active:scale-[0.97] ${s.hasChanges ? 'gradient-primary glow-primary text-white hover:scale-[1.01]' : 'bg-slate-100 text-text-muted cursor-not-allowed'}`}
                >
                    <span className={`material-symbols-outlined text-base ${s.isSaving ? 'animate-spin' : ''}`}>
                        {s.isSaving ? 'refresh' : 'save'}
                    </span>
                    {s.isSaving ? 'Saving...' : s.hasChanges ? 'Save Changes' : 'All Changes Saved'}
                </button>
                {s.saveStatus === 'saved' && (
                    <div className="flex items-center justify-center gap-2 text-sm text-green-600 animate-slide-up">
                        <span className="material-symbols-outlined text-base">check_circle</span>Settings saved successfully
                    </div>
                )}
                {s.saveStatus === 'error' && (
                    <div className="flex items-center justify-center gap-2 text-sm text-red-600 animate-slide-up">
                        <span className="material-symbols-outlined text-base">error</span>Failed to save. Please try again.
                    </div>
                )}
            </div>

            {/* ── Danger Zone ──────────────────────────── */}
            <section className="glass-card overflow-hidden border-l-4 border-l-red-400 section-enter stagger-8">
                <div className="px-5 py-4 border-b border-slate-100 bg-red-50/50">
                    <div className="flex items-center gap-3">
                        <div className="bg-red-100 p-2 rounded-xl">
                            <span className="material-symbols-outlined text-base text-red-600">warning</span>
                        </div>
                        <div>
                            <h3 className="text-sm font-bold text-text-main">Danger Zone</h3>
                            <p className="text-[11px] text-red-600/70 font-medium">Irreversible actions</p>
                        </div>
                    </div>
                </div>
                <div className="px-5 py-4 space-y-4">
                    <div>
                        <div className="flex items-start gap-3 mb-3">
                            <span className="material-symbols-outlined text-base text-text-muted mt-0.5">lock_clock</span>
                            <div>
                                <p className="text-sm font-semibold text-text-main">Day Closure</p>
                                <p className="text-xs text-text-muted">Finalize payroll, lock records, and close the harvest day</p>
                            </div>
                        </div>
                        <DayClosureButton />
                    </div>
                    <div className="border-t border-red-100 pt-4">
                        <div className="flex items-center justify-between">
                            <div className="flex items-start gap-3">
                                <span className="material-symbols-outlined text-base text-text-muted mt-0.5">delete_sweep</span>
                                <div>
                                    <p className="text-sm font-semibold text-text-main">Reset Today&apos;s Data</p>
                                    <p className="text-xs text-text-muted">Clear all bucket records for today</p>
                                </div>
                            </div>
                            <button className="px-3.5 py-2 text-sm font-semibold text-red-600 border border-red-200 rounded-xl hover:bg-red-50 transition-all active:scale-[0.96]">
                                Reset
                            </button>
                        </div>
                    </div>
                </div>
            </section>

            {/* ── Footer ──────────────────────────────── */}
            <div className="text-center py-4 space-y-1 section-enter stagger-8">
                <p className="text-xs font-bold text-text-muted">
                    HarvestPro NZ<span className="text-text-muted/50 mx-1.5">•</span>v9.0.0
                </p>
                <p className="text-[11px] text-text-muted/60">© 2026 HarvestPro. Built for NZ Orchards.</p>
            </div>
        </div>
    );
};

export default SettingsView;
