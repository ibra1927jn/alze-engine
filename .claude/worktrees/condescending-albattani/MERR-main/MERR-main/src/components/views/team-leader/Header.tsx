/**
 * Team Leader Header — Mobile header with notification bell
 */
import React, { useState } from 'react';
import { useMessaging } from '@/context/MessagingContext';
import NotificationPanel from '@/components/common/NotificationPanel';

interface HeaderProps {
    title: string;
    subtitle: string;
    onProfileClick: () => void;
    onNavigateToMessaging?: () => void;
}

const Header: React.FC<HeaderProps> = ({ title, subtitle, onProfileClick, onNavigateToMessaging }) => {
    const { unreadCount } = useMessaging();
    const [showNotifications, setShowNotifications] = useState(false);

    return (
        <header className="sticky top-0 z-30 glass-header">
            <div className="flex items-center px-4 py-3 justify-between">
                <div className="flex items-center gap-3">
                    {/* Logo mark */}
                    <div className="size-10 rounded-xl bg-primary shadow-md shadow-primary/20 flex items-center justify-center">
                        <span className="material-symbols-outlined text-white text-[22px]">agriculture</span>
                    </div>
                    <div>
                        <h1 className="text-text-main text-lg font-bold tracking-tight">{title}</h1>
                        <p className="text-xs text-text-muted">{subtitle}</p>
                    </div>
                </div>
                <div className="flex items-center gap-2">
                    <div className="relative">
                        <button
                            onClick={() => setShowNotifications(!showNotifications)}
                            className="size-10 rounded-xl bg-slate-100 border border-border-light flex items-center justify-center text-text-muted hover:bg-slate-200 transition-colors relative"
                        >
                            <span className="material-symbols-outlined text-[20px]">notifications</span>
                            {unreadCount > 0 ? (
                                <span className="absolute -top-1 -right-1 min-w-[18px] h-[18px] bg-red-500 text-white text-[10px] font-bold rounded-full flex items-center justify-center px-1">
                                    {unreadCount > 99 ? '99+' : unreadCount}
                                </span>
                            ) : (
                                <span className="absolute top-1.5 right-1.5 size-2 bg-primary rounded-full ring-2 ring-white"></span>
                            )}
                        </button>
                        {showNotifications && (
                            <NotificationPanel
                                onViewAll={onNavigateToMessaging}
                                onClose={() => setShowNotifications(false)}
                            />
                        )}
                    </div>
                    <button onClick={onProfileClick} className="size-10 rounded-xl bg-primary text-white flex items-center justify-center font-bold text-sm shadow-md shadow-primary/20 hover:bg-primary-dim transition-colors">TL</button>
                </div>
            </div>
        </header>
    );
};

export default Header;
