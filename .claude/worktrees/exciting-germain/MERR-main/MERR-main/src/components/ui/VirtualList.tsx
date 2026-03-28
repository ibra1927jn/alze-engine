import React from 'react';
import { Virtuoso } from 'react-virtuoso';

interface VirtualListProps<T> {
    items: T[];
    estimateSize: number;
    renderItem: (item: T, index: number) => React.ReactNode;
    className?: string;
    overscan?: number;
    getKey?: (item: T, index: number) => string | number;
}

/**
 * Generic virtual scroll wrapper powered by react-virtuoso.
 *
 * Usage:
 * ```tsx
 * <VirtualList
 *   items={pickers}
 *   estimateSize={72}
 *   renderItem={(picker) => <PickerCard picker={picker} />}
 * />
 * ```
 */
function VirtualList<T>({
    items,
    estimateSize,
    renderItem,
    className = '',
    overscan = 5,
    getKey,
}: VirtualListProps<T>) {
    if (items.length === 0) return null;

    return (
        <Virtuoso
            className={className}
            data={items}
            overscan={overscan}
            fixedItemHeight={estimateSize}
            computeItemKey={getKey ? (index, item) => getKey(item, index) : undefined}
            itemContent={(index, item) => renderItem(item, index)}
        />
    );
}

export default VirtualList;
