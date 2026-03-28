/**
 * NZST Timezone Utility
 * Forces all date/time operations to use New Zealand Standard Time (Pacific/Auckland)
 * Prevents UTC drift from toISOString() which causes 13-hour discrepancies
 */

const NZ_TIMEZONE = 'Pacific/Auckland';

/**
 * Get current date/time in NZST as an ISO-like string with offset.
 * Example: "2026-02-11T13:30:00+13:00"
 */
export function nowNZST(): string {
    const now = new Date();
    return toNZST(now);
}

/**
 * Get today's date in NZST as YYYY-MM-DD.
 * Critical: prevents the date from being "yesterday" due to UTC offset.
 */
export function todayNZST(): string {
    const formatter = new Intl.DateTimeFormat('en-CA', {
        timeZone: NZ_TIMEZONE,
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
    });
    return formatter.format(new Date()); // Returns "YYYY-MM-DD"
}

/**
 * Convert any Date to an ISO string in NZST with correct offset.
 */
export function toNZST(date: Date): string {
    // Get NZST components
    const parts = new Intl.DateTimeFormat('en-GB', {
        timeZone: NZ_TIMEZONE,
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false,
    }).formatToParts(date);

    const get = (type: string) => parts.find(p => p.type === type)?.value || '00';

    const iso = `${get('year')}-${get('month')}-${get('day')}T${get('hour')}:${get('minute')}:${get('second')}`;

    // Determine NZST (+12) vs NZDT (+13) offset
    const utcMs = date.getTime();
    const nzParts = new Intl.DateTimeFormat('en-US', {
        timeZone: NZ_TIMEZONE,
        year: 'numeric', month: 'numeric', day: 'numeric',
        hour: 'numeric', minute: 'numeric', second: 'numeric',
        hour12: false,
    }).formatToParts(date);

    const nzGet = (type: string) => parseInt(nzParts.find(p => p.type === type)?.value || '0');
    const nzDate = new Date(nzGet('year'), nzGet('month') - 1, nzGet('day'), nzGet('hour'), nzGet('minute'), nzGet('second'));
    const offsetMs = nzDate.getTime() - utcMs;
    const offsetHours = Math.round(offsetMs / 3600000);
    const offsetStr = `+${String(offsetHours).padStart(2, '0')}:00`;

    return `${iso}${offsetStr}`;
}
