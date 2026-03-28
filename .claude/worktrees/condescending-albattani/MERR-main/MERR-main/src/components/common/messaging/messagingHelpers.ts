/**
 * Messaging helpers, constants, and shared types
 */
import { MessagePriority } from '../../../types';

// ── Quick Reply presets for field context ──
export const QUICK_REPLIES = [
    { emoji: '👍', label: 'Acknowledged' },
    { emoji: '✅', label: 'Done' },
    { emoji: '🚜', label: 'On my way' },
    { emoji: '⚠️', label: 'Issue here' },
    { emoji: '☔', label: 'Weather stop' },
    { emoji: '🔄', label: 'Need backup' },
];

export const REACTION_EMOJIS = ['👍', '❤️', '😂', '😮', '😢', '🙏'];

// ── Avatar colors by name hash ──
const AVATAR_COLORS = [
    'from-violet-500 to-purple-600', 'from-blue-500 to-indigo-600', 'from-emerald-500 to-teal-600',
    'from-amber-500 to-orange-600', 'from-rose-500 to-pink-600', 'from-cyan-500 to-sky-600',
    'from-fuchsia-500 to-purple-600', 'from-lime-500 to-green-600',
];
export const getAvatarColor = (name: string) => AVATAR_COLORS[Math.abs(name.split('').reduce((a, c) => a + c.charCodeAt(0), 0)) % AVATAR_COLORS.length];

// ── Date formatting helpers ──
export const isSameDay = (d1: Date, d2: Date) =>
    d1.getFullYear() === d2.getFullYear() && d1.getMonth() === d2.getMonth() && d1.getDate() === d2.getDate();

export const getDateLabel = (dateStr: string) => {
    const d = new Date(dateStr);
    const now = new Date();
    if (isSameDay(d, now)) return 'Today';
    const yesterday = new Date(now);
    yesterday.setDate(now.getDate() - 1);
    if (isSameDay(d, yesterday)) return 'Yesterday';
    return d.toLocaleDateString('en-NZ', { weekday: 'short', day: 'numeric', month: 'short' });
};

export const formatTime = (dateStr: string) => {
    try {
        return new Date(dateStr).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    } catch { return ''; }
};

// ── Role badge helper (for NewChatModal) ──
export const getRoleBadge = (role: string) => {
    switch (role.toLowerCase()) {
        case 'manager': return { bg: 'bg-purple-100 text-purple-600', icon: 'admin_panel_settings' };
        case 'team_leader': return { bg: 'bg-blue-100 text-blue-600', icon: 'supervisor_account' };
        case 'runner': return { bg: 'bg-amber-100 text-amber-600', icon: 'directions_run' };
        default: return { bg: 'bg-emerald-100 text-emerald-600', icon: 'agriculture' };
    }
};

// ── Shared types for sub-components ──
export interface DBMessageCompat {
    id: string;
    sender_id: string;
    content: string;
    created_at: string;
    conversation_id?: string;
    priority: MessagePriority;
    read_by: string[];
    metadata?: { replyTo?: string };
}
