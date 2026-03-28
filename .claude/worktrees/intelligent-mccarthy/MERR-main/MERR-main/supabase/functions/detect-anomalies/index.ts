import { serve } from 'https://deno.land/std@0.168.0/http/server.ts'
import {
    handlePreflight,
    corsHeaders,
    requireRole,
    errorResponse,
    AnomalyInputSchema,
} from '../_shared/security.ts'

/**
 * detect-anomalies — Server-side Fraud Detection Edge Function
 *
 * Implements 4 anomaly rules that CANNOT be bypassed from the client:
 * 1. Elapsed-velocity: buckets ÷ time since last collection > physical max
 * 2. Peer comparison: picker rate > 3× row average
 * 3. Off-hours: scans outside shift window
 * 4. Duplicate proximity: same bin scanned by 2 pickers within 2 min
 */

// ── Detection Configuration ──────────────────────────
interface DetectionConfig {
    gracePeriodMinutes: number
    peerOutlierThreshold: number
    maxPhysicalRate: number
    postCollectionWindowMinutes: number
    shiftStartHour: number
    shiftEndHour: number
}

const CONFIG: DetectionConfig = {
    gracePeriodMinutes: 90,
    peerOutlierThreshold: 3.0,
    maxPhysicalRate: 8,
    postCollectionWindowMinutes: 20,
    shiftStartHour: 6,
    shiftEndHour: 19,
}

// ── Types ────────────────────────────────────────────
type AnomalyType =
    | 'impossible_velocity'
    | 'peer_outlier'
    | 'off_hours'
    | 'duplicate_proximity'
    | 'post_collection_spike'

interface Anomaly {
    id: string
    type: AnomalyType
    severity: 'low' | 'medium' | 'high'
    pickerId: string
    pickerName: string
    detail: string
    timestamp: string
    evidence: Record<string, unknown>
    rule: 'elapsed_velocity' | 'peer_comparison' | 'off_hours' | 'duplicate'
}

interface BucketEvent {
    id: string
    picker_id: string
    row_number: number | null
    recorded_at: string
    users: { name: string } | null
}

serve(async (req) => {
    // ── CORS Preflight ───────────────────────────────
    const preflight = handlePreflight(req)
    if (preflight) return preflight

    const origin = req.headers.get('Origin')

    try {
        // ── Auth + RBAC ──────────────────────────────
        const { supabase } = await requireRole(req, ['owner', 'manager', 'supervisor'])

        // ── Input Validation ─────────────────────────
        const body = await req.json()
        const { orchard_id } = AnomalyInputSchema.parse(body)

        // Get today's boundaries in NZST
        const now = new Date()
        const todayStart = new Date(now)
        todayStart.setHours(0, 0, 0, 0)

        // Fetch today's bucket events with picker info
        const { data: events, error: eventsError } = await supabase
            .from('bucket_events')
            .select('id, picker_id, row_number, recorded_at, users(name)')
            .eq('orchard_id', orchard_id)
            .gte('recorded_at', todayStart.toISOString())
            .order('recorded_at', { ascending: true })

        if (eventsError) throw new Error(`Failed to fetch events: ${eventsError.message}`)
        if (!events || events.length === 0) {
            return new Response(JSON.stringify({ anomalies: [], stats: { total: 0 } }), {
                headers: { ...corsHeaders(origin), 'Content-Type': 'application/json' },
            })
        }

        const anomalies: Anomaly[] = []
        let anomalyCounter = 0

        const makeId = () => `anm-${++anomalyCounter}-${Date.now()}`

        // ── Group events by picker ────────────────────
        const pickerEvents = new Map<string, BucketEvent[]>()
        const pickerNames = new Map<string, string>()

        for (const event of events) {
            const pid = event.picker_id
            // deno-lint-ignore no-explicit-any
            const name = (event.users as any)?.name || 'Unknown'
            pickerNames.set(pid, name)
            if (!pickerEvents.has(pid)) pickerEvents.set(pid, [])
            pickerEvents.get(pid)!.push(event)
        }

        // ── Rule 1: Off-Hours Detection ──────────────
        for (const [pickerId, pEvents] of pickerEvents) {
            for (const event of pEvents) {
                const scanTime = new Date(event.recorded_at)
                const hour = scanTime.getHours()
                if (hour < CONFIG.shiftStartHour || hour >= CONFIG.shiftEndHour) {
                    anomalies.push({
                        id: makeId(),
                        type: 'off_hours',
                        severity: 'medium',
                        pickerId,
                        pickerName: pickerNames.get(pickerId) || 'Unknown',
                        detail: `Scan at ${scanTime.toLocaleTimeString('en-NZ', { hour: '2-digit', minute: '2-digit' })} — outside shift window (${CONFIG.shiftStartHour}:00-${CONFIG.shiftEndHour}:00).`,
                        timestamp: event.recorded_at,
                        evidence: {
                            scanTime: scanTime.toISOString(),
                            shiftStart: `${CONFIG.shiftStartHour}:00`,
                            shiftEnd: `${CONFIG.shiftEndHour}:00`,
                        },
                        rule: 'off_hours',
                    })
                    break // Only flag once per picker per day
                }
            }
        }

        // ── Rule 2: Impossible Velocity ──────────────
        for (const [pickerId, pEvents] of pickerEvents) {
            if (pEvents.length < 2) continue

            const firstScan = new Date(pEvents[0].recorded_at)
            const lastScan = new Date(pEvents[pEvents.length - 1].recorded_at)
            const hoursElapsed = (lastScan.getTime() - firstScan.getTime()) / (1000 * 60 * 60)

            if (hoursElapsed < 0.1) continue

            const rate = pEvents.length / hoursElapsed

            const shiftStart = new Date(now)
            shiftStart.setHours(CONFIG.shiftStartHour, 0, 0, 0)
            const minutesSinceShiftStart = (firstScan.getTime() - shiftStart.getTime()) / (1000 * 60)
            const inGracePeriod = minutesSinceShiftStart < CONFIG.gracePeriodMinutes

            const threshold = inGracePeriod
                ? CONFIG.maxPhysicalRate * 2.5
                : CONFIG.maxPhysicalRate

            if (rate > threshold) {
                anomalies.push({
                    id: makeId(),
                    type: 'impossible_velocity',
                    severity: rate > CONFIG.maxPhysicalRate * 2 ? 'high' : 'medium',
                    pickerId,
                    pickerName: pickerNames.get(pickerId) || 'Unknown',
                    detail: `${pEvents.length} buckets in ${(hoursElapsed * 60).toFixed(0)} min (${rate.toFixed(1)}/hr). Physical max: ${CONFIG.maxPhysicalRate}/hr.${inGracePeriod ? ' Grace period active — flagged as extreme outlier.' : ''}`,
                    timestamp: lastScan.toISOString(),
                    evidence: {
                        buckets: pEvents.length,
                        minutesWorked: Math.round(hoursElapsed * 60),
                        impliedRate: parseFloat(rate.toFixed(1)),
                        maxPhysicalRate: CONFIG.maxPhysicalRate,
                        inGracePeriod,
                    },
                    rule: 'elapsed_velocity',
                })
            }
        }

        // ── Rule 3: Peer Comparison (by row) ─────────
        const rowGroups = new Map<number, { pickerId: string; rate: number; buckets: number }[]>()

        for (const [pickerId, pEvents] of pickerEvents) {
            const rowCounts = new Map<number, number>()
            for (const ev of pEvents) {
                if (ev.row_number != null) {
                    rowCounts.set(ev.row_number, (rowCounts.get(ev.row_number) || 0) + 1)
                }
            }

            if (rowCounts.size === 0) continue

            let dominantRow = 0
            let maxCount = 0
            for (const [row, count] of rowCounts) {
                if (count > maxCount) { dominantRow = row; maxCount = count }
            }

            const firstScan = new Date(pEvents[0].recorded_at)
            const lastScan = new Date(pEvents[pEvents.length - 1].recorded_at)
            const hours = (lastScan.getTime() - firstScan.getTime()) / (1000 * 60 * 60)
            if (hours < 0.5) continue

            const rate = pEvents.length / hours

            if (!rowGroups.has(dominantRow)) rowGroups.set(dominantRow, [])
            rowGroups.get(dominantRow)!.push({ pickerId, rate, buckets: pEvents.length })
        }

        for (const [rowNum, pickers] of rowGroups) {
            if (pickers.length < 2) continue

            const avgRate = pickers.reduce((sum, p) => sum + p.rate, 0) / pickers.length

            for (const picker of pickers) {
                const multiplier = picker.rate / avgRate
                if (multiplier >= CONFIG.peerOutlierThreshold) {
                    const peers = pickers
                        .filter(p => p.pickerId !== picker.pickerId)
                        .map(p => `${pickerNames.get(p.pickerId)} (${p.rate.toFixed(1)}/hr)`)

                    anomalies.push({
                        id: makeId(),
                        type: 'peer_outlier',
                        severity: multiplier >= 4 ? 'high' : 'medium',
                        pickerId: picker.pickerId,
                        pickerName: pickerNames.get(picker.pickerId) || 'Unknown',
                        detail: `${picker.rate.toFixed(1)} bins/hr while row mates average ${avgRate.toFixed(1)}/hr (${multiplier.toFixed(1)}× faster). Same trees, same conditions.`,
                        timestamp: now.toISOString(),
                        evidence: {
                            pickerRate: parseFloat(picker.rate.toFixed(1)),
                            rowAverage: parseFloat(avgRate.toFixed(1)),
                            multiplier: parseFloat(multiplier.toFixed(1)),
                            rowId: `Row ${rowNum}`,
                            rowPeers: peers,
                        },
                        rule: 'peer_comparison',
                    })
                }
            }
        }

        // ── Rule 4: Duplicate Proximity ──────────────
        const eventsByTime = [...events].sort(
            (a, b) => new Date(a.recorded_at).getTime() - new Date(b.recorded_at).getTime()
        )

        for (let i = 0; i < eventsByTime.length; i++) {
            for (let j = i + 1; j < eventsByTime.length; j++) {
                const a = eventsByTime[i]
                const b = eventsByTime[j]

                const timeDiff = new Date(b.recorded_at).getTime() - new Date(a.recorded_at).getTime()
                if (timeDiff > 120_000) break

                if (a.row_number != null && a.row_number === b.row_number && a.picker_id !== b.picker_id) {
                    anomalies.push({
                        id: makeId(),
                        type: 'duplicate_proximity',
                        severity: 'medium',
                        pickerId: b.picker_id,
                        pickerName: pickerNames.get(b.picker_id) || 'Unknown',
                        detail: `Scan on Row ${a.row_number} — same row was scanned by ${pickerNames.get(a.picker_id)} ${Math.round(timeDiff / 1000)}s earlier. Possible tag sharing.`,
                        timestamp: b.recorded_at,
                        evidence: {
                            rowNumber: a.row_number,
                            conflictPicker: pickerNames.get(a.picker_id),
                            timeDiffSeconds: Math.round(timeDiff / 1000),
                        },
                        rule: 'duplicate',
                    })
                }
            }
        }

        // Sort by severity (high first)
        const severityOrder: Record<string, number> = { high: 0, medium: 1, low: 2 }
        anomalies.sort((a, b) => (severityOrder[a.severity] ?? 9) - (severityOrder[b.severity] ?? 9))

        console.log(`[detect-anomalies] Found ${anomalies.length} anomalies for orchard ${orchard_id}`)

        return new Response(JSON.stringify({
            anomalies,
            stats: {
                total: anomalies.length,
                high: anomalies.filter(a => a.severity === 'high').length,
                medium: anomalies.filter(a => a.severity === 'medium').length,
                low: anomalies.filter(a => a.severity === 'low').length,
            },
        }), {
            headers: { ...corsHeaders(origin), 'Content-Type': 'application/json' },
        })

    } catch (error) {
        return errorResponse(error, origin, 'detect-anomalies')
    }
})
