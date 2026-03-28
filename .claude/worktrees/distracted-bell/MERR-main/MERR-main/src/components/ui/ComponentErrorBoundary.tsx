/**
 * ComponentErrorBoundary.tsx — Inline Error Boundary for Widgets
 * 
 * Unlike the full-page ErrorBoundary, this renders an inline fallback card
 * so a single broken component doesn't take down the entire page.
 * Wrap analytics charts, cost views, and any component that processes
 * complex data that could throw (division by zero, null refs, etc).
 */
import { logger } from '@/utils/logger';
import React, { Component, ErrorInfo, ReactNode } from 'react';

interface Props {
    children: ReactNode;
    /** Friendly name shown in the fallback UI */
    componentName?: string;
}

interface State {
    hasError: boolean;
    error: Error | null;
}

class ComponentErrorBoundary extends Component<Props, State> {
    public state: State = {
        hasError: false,
        error: null,
    };

    public static getDerivedStateFromError(error: Error): State {
        return { hasError: true, error };
    }

    public componentDidCatch(error: Error, errorInfo: ErrorInfo) {
        logger.error(
            `[ComponentErrorBoundary] ${this.props.componentName || 'Component'} crashed:`,
            error,
            errorInfo
        );
    }

    private handleRetry = () => {
        this.setState({ hasError: false, error: null });
    };

    public render() {
        if (this.state.hasError) {
            const name = this.props.componentName || 'This section';
            return (
                <div className="bg-white rounded-2xl shadow-sm border border-red-100 overflow-hidden">
                    <div className="p-6 flex flex-col items-center text-center">
                        {/* Icon */}
                        <div className="w-12 h-12 rounded-full bg-red-50 flex items-center justify-center mb-3">
                            <span className="material-symbols-outlined text-2xl text-red-500">error_outline</span>
                        </div>

                        {/* Title */}
                        <h4 className="text-sm font-bold text-text-main mb-1">
                            {name} failed to load
                        </h4>
                        <p className="text-xs text-text-muted mb-4 max-w-[250px]">
                            An unexpected error occurred. Other parts of the app are unaffected.
                        </p>

                        {/* Error detail (dev mode only) */}
                        {import.meta.env.DEV && this.state.error && (
                            <div className="w-full bg-slate-50 rounded-lg p-2 mb-4 text-left overflow-auto max-h-20">
                                <code className="text-[10px] text-red-500 font-mono break-all">
                                    {this.state.error.message}
                                </code>
                            </div>
                        )}

                        {/* Retry button */}
                        <button
                            onClick={this.handleRetry}
                            className="px-4 py-2 bg-text-main text-white text-xs font-bold rounded-xl hover:opacity-90 transition-opacity flex items-center gap-1.5"
                        >
                            <span className="material-symbols-outlined text-sm">refresh</span>
                            Retry
                        </button>
                    </div>
                </div>
            );
        }

        return this.props.children;
    }
}

export default ComponentErrorBoundary;
