/**
 * useSettings — State management and handlers for SettingsView
 */
import { useState, useEffect, useCallback } from 'react';
import { useHarvestStore } from '@/stores/useHarvestStore';
import { settingsService } from '@/services/settings.service';
import { notificationService } from '@/services/notification.service';
import { logger } from '@/utils/logger';

export interface SettingsFormData {
    piece_rate: number;
    min_wage_rate: number;
    min_buckets_per_hour: number;
    target_tons: number;
    variety: string;
}

export interface ComplianceToggles {
    nz_employment_standards: boolean;
    auto_wage_alerts: boolean;
    safety_verification: boolean;
    audit_trail: boolean;
}

export function useSettings() {
    const { orchard, settings, updateSettings, currentUser } = useHarvestStore();
    const orchardId = orchard?.id;

    // Form data
    const [formData, setFormData] = useState<SettingsFormData>({
        piece_rate: settings?.piece_rate ?? 6.5,
        min_wage_rate: settings?.min_wage_rate ?? 23.50,
        min_buckets_per_hour: settings?.min_buckets_per_hour ?? 8,
        target_tons: settings?.target_tons ?? 40,
        variety: settings?.variety ?? 'Cherry',
    });

    const [compliance, setCompliance] = useState<ComplianceToggles>({
        nz_employment_standards: true,
        auto_wage_alerts: true,
        safety_verification: true,
        audit_trail: true,
    });

    // Notification state
    const [notifEnabled, setNotifEnabled] = useState(() => notificationService.getPrefs().enabled);
    const [notifTypes, setNotifTypes] = useState(() => notificationService.getPrefs().types);
    const [notifTestSent, setNotifTestSent] = useState(false);

    // Save state
    const [isSaving, setIsSaving] = useState(false);
    const [saveStatus, setSaveStatus] = useState<'idle' | 'saved' | 'error'>('idle');
    const [hasChanges, setHasChanges] = useState(false);

    // Sync from store
    useEffect(() => {
        if (settings) {
            setFormData({
                piece_rate: settings.piece_rate,
                min_wage_rate: settings.min_wage_rate,
                min_buckets_per_hour: settings.min_buckets_per_hour,
                target_tons: settings.target_tons,
                variety: settings.variety ?? 'Cherry',
            });
        }
    }, [settings]);

    const handleChange = useCallback((field: keyof SettingsFormData, value: string | number) => {
        setFormData(prev => ({ ...prev, [field]: value }));
        setHasChanges(true);
        setSaveStatus('idle');
    }, []);

    const handleSave = async () => {
        if (!orchardId) return;
        setIsSaving(true);
        setSaveStatus('idle');
        try {
            const payload = {
                piece_rate: Number(formData.piece_rate),
                min_wage_rate: Number(formData.min_wage_rate),
                min_buckets_per_hour: Number(formData.min_buckets_per_hour),
                target_tons: Number(formData.target_tons),
                variety: formData.variety,
            };
            await settingsService.updateHarvestSettings(orchardId, payload);
            updateSettings(payload);
            setSaveStatus('saved');
            setHasChanges(false);
        } catch (e) {
            logger.error('[SettingsView] Failed to save settings:', e);
            setSaveStatus('error');
        } finally {
            setIsSaving(false);
        }
    };

    const handleNotifToggle = async (enabled: boolean) => {
        const ok = await notificationService.setEnabled(enabled);
        if (ok) setNotifEnabled(enabled);
    };

    const handleNotifType = (type: string, checked: boolean) => {
        const updated = { ...notifTypes, [type]: checked };
        setNotifTypes(updated);
        notificationService.setAlertTypes({ [type]: checked } as Record<string, boolean>);
    };

    const handleSendTest = () => {
        notificationService.sendTest();
        setNotifTestSent(true);
        setTimeout(() => setNotifTestSent(false), 3000);
    };

    const initials = (currentUser?.name || 'M')
        .split(' ')
        .map(w => w[0])
        .join('')
        .toUpperCase()
        .slice(0, 2);

    return {
        // Context
        orchard, currentUser, initials,
        // Form
        formData, handleChange,
        compliance, setCompliance,
        // Save
        isSaving, saveStatus, hasChanges, handleSave,
        // Notifications
        notifEnabled, notifTypes, notifTestSent,
        handleNotifToggle, handleNotifType, handleSendTest,
    };
}
