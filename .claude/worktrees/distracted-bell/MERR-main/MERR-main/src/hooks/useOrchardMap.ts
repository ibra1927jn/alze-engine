/**
 * useOrchardMap — Data logic and computations for OrchardMapView
 */
import { useMemo } from 'react';
import { Picker, BucketRecord, OrchardBlock } from '@/types';
import { useHarvestStore } from '@/stores/useHarvestStore';
import { getRowProgress } from '@/utils/orchardMapUtils';

export interface RowData {
    rowNum: number;
    buckets: number;
    pickers: Picker[];
    variety: string;
    progress: number;
}

export interface BlockStats {
    buckets: number;
    activePickers: number;
    completedRows: number;
    progress: number;
}

export interface OrchardMapData {
    // Store
    orchardBlocks: OrchardBlock[];
    selectedBlock: OrchardBlock | null;
    selectedBlockId: string | null;
    selectedVariety: string;
    setSelectedBlock: (id: string | null) => void;
    setSelectedVariety: (v: string) => void;
    orchardName: string;
    rowAssignments: ReturnType<typeof useHarvestStore> extends { rowAssignments: infer R } ? R : unknown[];

    // Computed
    blockStats: Record<string, BlockStats>;
    blockVarietySummaries: Record<string, string[]>;
    blockVarieties: string[];
    rowData: RowData[];
    pickersByRow: Record<number, Picker[]>;

    // Global stats
    totalActivePickers: number;
    totalBuckets: number;
    totalRows: number;
}

export function useOrchardMap(
    crew: Picker[],
    bucketRecords: BucketRecord[],
    targetBucketsPerRow: number,
): OrchardMapData {
    const orchardBlocks = useHarvestStore(s => s.orchardBlocks);
    const selectedBlockId = useHarvestStore(s => s.selectedBlockId);
    const selectedVariety = useHarvestStore(s => s.selectedVariety);
    const setSelectedBlock = useHarvestStore(s => s.setSelectedBlock);
    const setSelectedVariety = useHarvestStore(s => s.setSelectedVariety);
    const orchard = useHarvestStore(s => s.orchard);
    const rowAssignments = useHarvestStore(s => s.rowAssignments);

    const selectedBlock = orchardBlocks.find(b => b.id === selectedBlockId) || null;

    // Pickers-by-row lookup
    const pickersByRow = useMemo(() => {
        const map: Record<number, Picker[]> = {};
        rowAssignments.forEach(ra => {
            if (!map[ra.row_number]) map[ra.row_number] = [];
            ra.assigned_pickers.forEach(pid => {
                const p = crew.find(c => c.id === pid);
                if (p && !map[ra.row_number].find(x => x.id === p.id)) {
                    map[ra.row_number].push(p);
                }
            });
        });
        crew.forEach(p => {
            if (p.current_row > 0) {
                if (!map[p.current_row]) map[p.current_row] = [];
                if (!map[p.current_row].find(x => x.id === p.id)) {
                    map[p.current_row].push(p);
                }
            }
        });
        return map;
    }, [rowAssignments, crew]);

    // Varieties for selected block
    const blockVarieties = useMemo(() => {
        if (!selectedBlock) return [];
        return Array.from(new Set(Object.values(selectedBlock.rowVarieties)));
    }, [selectedBlock]);

    // Variety summaries per block (for Level 1 badges)
    const blockVarietySummaries = useMemo(() => {
        const summaries: Record<string, string[]> = {};
        orchardBlocks.forEach(block => {
            summaries[block.id] = Array.from(new Set(Object.values(block.rowVarieties)));
        });
        return summaries;
    }, [orchardBlocks]);

    // Block-level stats
    const blockStats = useMemo(() => {
        const stats: Record<string, BlockStats> = {};
        orchardBlocks.forEach(block => {
            let blockBuckets = 0, blockPickers = 0, blockCompletedRows = 0;
            for (let row = block.startRow; row < block.startRow + block.totalRows; row++) {
                const rowBuckets = bucketRecords.filter(br => br.row_number === row).length;
                blockBuckets += rowBuckets;
                blockPickers += (pickersByRow[row] || []).length;
                if (rowBuckets >= targetBucketsPerRow) blockCompletedRows++;
            }
            stats[block.id] = {
                buckets: blockBuckets,
                activePickers: blockPickers,
                completedRows: blockCompletedRows,
                progress: block.totalRows > 0 ? blockCompletedRows / block.totalRows : 0,
            };
        });
        return stats;
    }, [orchardBlocks, bucketRecords, targetBucketsPerRow, pickersByRow]);

    // Row data for selected block
    const rowData = useMemo(() => {
        if (!selectedBlock) return [];
        return Array.from({ length: selectedBlock.totalRows }, (_, i) => {
            const rowNum = selectedBlock.startRow + i;
            const buckets = bucketRecords.filter(br => br.row_number === rowNum).length;
            const pickers = pickersByRow[rowNum] || [];
            const variety = selectedBlock.rowVarieties[rowNum] || 'Unknown';
            return { rowNum, buckets, pickers, variety, progress: getRowProgress(buckets, targetBucketsPerRow) };
        });
    }, [selectedBlock, bucketRecords, targetBucketsPerRow, pickersByRow]);

    // Global stats
    const totalActivePickers = crew.filter(p => p.status === 'active').length;
    const totalBuckets = bucketRecords.length;
    const totalRows = orchardBlocks.reduce((s, b) => s + b.totalRows, 0);

    return {
        orchardBlocks, selectedBlock, selectedBlockId, selectedVariety,
        setSelectedBlock, setSelectedVariety,
        orchardName: orchard?.name || 'Orchard',
        rowAssignments,
        blockStats, blockVarietySummaries, blockVarieties,
        rowData, pickersByRow,
        totalActivePickers, totalBuckets, totalRows,
    };
}
