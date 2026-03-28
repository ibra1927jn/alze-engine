/**
 * orchardMapSlice — Orchard Map state (The Shared Brain)
 *
 * Manages block selection, variety filtering, and block data.
 * All 3 map views (Tactical, HeatMap, RowList) read from this single slice —
 * "dumb monitors" fed by one brain.
 *
 * KEY: Blocks and rows are fetched FROM SUPABASE (harvest_seasons → orchard_blocks → block_rows).
 * No more mock data. The active season scopes all queries.
 */
import type { OrchardBlock } from '@/types';
import type { StateCreator } from 'zustand';
import type { HarvestStoreState } from '../storeTypes';
import { supabase } from '@/services/supabase';
import { logger } from '@/utils/logger';

// --- Slice Interface ---
export interface OrchardMapSlice {
    orchardBlocks: OrchardBlock[];
    selectedBlockId: string | null;
    selectedVariety: string | 'ALL';
    activeSeasonId: string | null;
    blocksLoading: boolean;
    setSelectedBlock: (id: string | null) => void;
    setSelectedVariety: (variety: string | 'ALL') => void;
    fetchBlocks: (orchardId: string) => Promise<void>;
}

// --- Slice Creator ---
export const createOrchardMapSlice: StateCreator<
    HarvestStoreState,
    [],
    [],
    OrchardMapSlice
> = (set) => ({
    orchardBlocks: [],
    selectedBlockId: null,
    selectedVariety: 'ALL',
    activeSeasonId: null,
    blocksLoading: false,

    setSelectedBlock: (id) => set({ selectedBlockId: id, selectedVariety: 'ALL' }),
    setSelectedVariety: (variety) => set({ selectedVariety: variety }),

    /**
     * Fetch real blocks from Supabase:
     * 1. Get the active season for this orchard
     * 2. Get all blocks in that season
     * 3. Get all rows for each block (to build rowVarieties map)
     * 4. Transform into OrchardBlock[] shape the UI expects
     */
    fetchBlocks: async (orchardId: string) => {
        set({ blocksLoading: true });
        try {
            // Step 1: Get active season
            const { data: seasons, error: seasonErr } = await supabase
                .from('harvest_seasons')
                .select('id, name, start_date, end_date, status')
                .eq('orchard_id', orchardId)
                .eq('status', 'active')
                .is('deleted_at', null)
                .limit(1);

            if (seasonErr) {
                logger.error('[OrchardMap] Error fetching season:', seasonErr);
                set({ blocksLoading: false });
                return;
            }

            const activeSeason = seasons?.[0];
            if (!activeSeason) {
                logger.warn('[OrchardMap] No active season found for orchard:', orchardId);
                set({ orchardBlocks: [], activeSeasonId: null, blocksLoading: false });
                return;
            }

            // Step 2: Get all blocks for this season
            const { data: blocks, error: blockErr } = await supabase
                .from('orchard_blocks')
                .select('id, name, total_rows, start_row, color_code, status')
                .eq('orchard_id', orchardId)
                .eq('season_id', activeSeason.id)
                .is('deleted_at', null)
                .order('start_row', { ascending: true });

            if (blockErr) {
                logger.error('[OrchardMap] Error fetching blocks:', blockErr);
                set({ blocksLoading: false });
                return;
            }

            if (!blocks || blocks.length === 0) {
                logger.info('[OrchardMap] No blocks found — orchard may need setup');
                set({ orchardBlocks: [], activeSeasonId: activeSeason.id, blocksLoading: false });
                return;
            }

            // Step 3: Get all rows for all blocks in one query
            const blockIds = blocks.map(b => b.id);
            const { data: rows, error: rowErr } = await supabase
                .from('block_rows')
                .select('id, block_id, row_number, variety')
                .in('block_id', blockIds)
                .is('deleted_at', null)
                .order('row_number', { ascending: true });

            if (rowErr) {
                logger.error('[OrchardMap] Error fetching rows:', rowErr);
            }

            // Step 4: Build rowVarieties map per block
            const rowsByBlock = new Map<string, Record<number, string>>();
            for (const row of (rows || [])) {
                if (!rowsByBlock.has(row.block_id)) {
                    rowsByBlock.set(row.block_id, {});
                }
                rowsByBlock.get(row.block_id)![row.row_number] = row.variety || 'Desconocida';
            }

            // Step 5: Transform to OrchardBlock[] shape
            const orchardBlocks: OrchardBlock[] = blocks.map(block => ({
                id: block.id,
                name: block.name,
                totalRows: block.total_rows,
                startRow: block.start_row,
                colorCode: block.color_code || '#dc2626',
                status: block.status as OrchardBlock['status'],
                rowVarieties: rowsByBlock.get(block.id) || {},
            }));

            set({
                orchardBlocks,
                activeSeasonId: activeSeason.id,
                blocksLoading: false,
            });

            logger.info(`[OrchardMap] Loaded ${orchardBlocks.length} blocks, ${(rows || []).length} rows from season "${activeSeason.name}"`);
        } catch (error) {
            logger.error('[OrchardMap] Unexpected error fetching blocks:', error);
            set({ blocksLoading: false });
        }
    },
});
