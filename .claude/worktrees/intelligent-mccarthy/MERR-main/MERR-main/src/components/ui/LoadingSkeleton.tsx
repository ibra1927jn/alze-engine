import React from 'react';

interface LoadingSkeletonProps {
    type?: 'card' | 'list' | 'table' | 'metric' | 'text';
    count?: number;
    height?: string;
    className?: string;
}

/**
 * LoadingSkeleton - Reusable skeleton loader component
 * 
 * Eliminates "¿Está cargando o se trabó?" uncertainty for field workers.
 * 
 * @param type - Type of skeleton: card, list, table, metric, text
 * @param count - Number of skeleton items to render
 * @param height - Custom height (default varies by type)
 * @param className - Additional CSS classes
 */
const LoadingSkeleton: React.FC<LoadingSkeletonProps> = ({
    type = 'card',
    count = 1,
    height: _height, // Renamed to _height as it's not directly used in the switch logic
    className = ''
}) => {
    const getSkeletonElement = () => {
        switch (type) {
            case 'metric':
                return (
                    <div className={`bg-white rounded-lg p-4 shadow ${className}`}>
                        <div>
                            <div className="h-4 animate-shimmer rounded w-1/2 mb-2"></div>
                            <div className="h-8 animate-shimmer rounded w-3/4"></div>
                        </div>
                    </div>
                );

            case 'card':
                return (
                    <div className={`bg-white rounded-lg p-4 shadow h-[120px] ${className}`}>
                        <div className="flex flex-col h-full">
                            <div className="h-4 animate-shimmer rounded w-3/4 mb-3"></div>
                            <div className="h-3 animate-shimmer rounded w-1/2 mb-2"></div>
                            <div className="h-3 animate-shimmer rounded w-2/3 mt-auto"></div>
                        </div>
                    </div>
                );

            case 'list':
                return (
                    <div className={`bg-white rounded-lg p-3 mb-2 shadow-sm ${className}`}>
                        <div className="flex items-center gap-3">
                            <div className="w-10 h-10 animate-shimmer rounded-full flex-shrink-0"></div>
                            <div className="flex-1">
                                <div className="h-4 animate-shimmer rounded w-3/4 mb-2"></div>
                                <div className="h-3 animate-shimmer rounded w-1/2"></div>
                            </div>
                        </div>
                    </div>
                );

            case 'table':
                return (
                    <div className={`bg-white border-b border-border-light ${className}`}>
                        <div className="flex items-center gap-4 p-3">
                            <div className="h-4 animate-shimmer rounded w-1/6"></div>
                            <div className="h-4 animate-shimmer rounded w-1/4"></div>
                            <div className="h-4 animate-shimmer rounded w-1/6"></div>
                            <div className="h-4 animate-shimmer rounded w-1/6"></div>
                        </div>
                    </div>
                );

            case 'text':
                return (
                    <div className={className}>
                        <div className="h-4 animate-shimmer rounded w-full"></div>
                    </div>
                );

            default:
                return <div className="h-20 animate-shimmer rounded"></div>;
        }
    };

    return (
        <>
            {Array.from({ length: count }).map((_, index) => (
                <React.Fragment key={index}>
                    {getSkeletonElement()}
                </React.Fragment>
            ))}
        </>
    );
};

export default LoadingSkeleton;
