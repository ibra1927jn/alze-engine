/**
 * Profile Modal - Shared component for user profile settings
 */
import React, { useState } from 'react';
import { useAuth } from '../../context/AuthContext';
import ModalOverlay from '@/components/ui/ModalOverlay';

interface ProfileModalProps {
    onClose: () => void;
    onLogout: () => void;
    roleLabel?: string;
    stats?: {
        bucketsHandled?: number;
        binsCompleted?: number;
        pickersManaged?: number;
        rowsCompleted?: number;
    };
}

const ProfileModal: React.FC<ProfileModalProps> = ({
    onClose,
    onLogout,
    roleLabel = 'User',
    stats,
}) => {
    const { appUser } = useAuth();
    const [isEditing, setIsEditing] = useState(false);
    const [name, setName] = useState(appUser?.full_name || 'User');

    const handleSave = () => {
        setIsEditing(false);
    };

    return (
        <ModalOverlay onClose={onClose}>
            <div className="p-6 max-h-[85vh] overflow-y-auto">
                <div className="flex items-center justify-between mb-6">
                    <h3 className="text-xl font-black text-text-main">Profile Settings</h3>
                    <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                <div className="flex flex-col items-center mb-6">
                    <div className="size-20 rounded-full bg-primary text-white flex items-center justify-center text-3xl font-bold mb-3">
                        {name.charAt(0)}
                    </div>
                    {isEditing ? (
                        <input
                            type="text"
                            value={name}
                            onChange={e => setName(e.target.value)}
                            aria-label="Full Name"
                            placeholder="Enter your name"
                            className="text-center text-xl font-bold px-4 py-2 border-2 border-border-light rounded-lg focus:border-primary outline-none transition-colors"
                        />
                    ) : (
                        <h4 className="text-xl font-black text-text-main">{name}</h4>
                    )}
                    <p className="text-sm text-text-muted mt-1">{roleLabel}</p>
                </div>

                <div className="space-y-3 mb-6">
                    <div className="bg-slate-50 rounded-xl p-4 border border-border-light">
                        <p className="text-xs font-bold text-text-muted uppercase mb-1">Email</p>
                        <p className="text-sm font-medium text-text-main">
                            {appUser?.email || 'user@harvestpro.nz'}
                        </p>
                    </div>
                    <div className="bg-slate-50 rounded-xl p-4 border border-border-light">
                        <p className="text-xs font-bold text-text-muted uppercase mb-1">PIN</p>
                        <p className="text-sm font-medium text-text-main">••••</p>
                    </div>
                    {stats && (
                        <div className="bg-primary-light rounded-xl p-4 border border-primary/20">
                            <p className="text-xs font-bold text-primary uppercase mb-1">Today's Stats</p>
                            <div className="grid grid-cols-2 gap-3 mt-2">
                                {stats.bucketsHandled !== undefined && (
                                    <div>
                                        <p className="text-2xl font-black text-text-main">{stats.bucketsHandled}</p>
                                        <p className="text-xs text-text-sub">Buckets Moved</p>
                                    </div>
                                )}
                                {stats.binsCompleted !== undefined && (
                                    <div>
                                        <p className="text-2xl font-black text-text-main">{stats.binsCompleted}</p>
                                        <p className="text-xs text-text-sub">Bins Completed</p>
                                    </div>
                                )}
                                {stats.pickersManaged !== undefined && (
                                    <div>
                                        <p className="text-2xl font-black text-text-main">{stats.pickersManaged}</p>
                                        <p className="text-xs text-text-sub">Pickers</p>
                                    </div>
                                )}
                                {stats.rowsCompleted !== undefined && (
                                    <div>
                                        <p className="text-2xl font-black text-text-main">{stats.rowsCompleted}</p>
                                        <p className="text-xs text-text-sub">Rows Completed</p>
                                    </div>
                                )}
                            </div>
                        </div>
                    )}
                </div>

                <div className="space-y-2">
                    {isEditing ? (
                        <>
                            <button
                                onClick={handleSave}
                                className="w-full py-3 gradient-primary glow-primary text-white rounded-xl font-bold"
                            >
                                Save Changes
                            </button>
                            <button
                                onClick={() => setIsEditing(false)}
                                className="w-full py-3 bg-slate-100 text-text-sub rounded-xl font-bold hover:bg-slate-200 transition-colors"
                            >
                                Cancel
                            </button>
                        </>
                    ) : (
                        <>
                            <button
                                onClick={() => setIsEditing(true)}
                                className="w-full py-3 bg-slate-100 text-text-sub rounded-xl font-bold flex items-center justify-center gap-2 hover:bg-slate-200 transition-colors"
                            >
                                <span className="material-symbols-outlined">edit</span>
                                Edit Profile
                            </button>
                            <button
                                onClick={() => {
                                    if (confirm('Are you sure you want to logout?')) {
                                        onLogout();
                                    }
                                }}
                                className="w-full py-3 bg-red-50 text-danger border-2 border-red-200 rounded-xl font-bold flex items-center justify-center gap-2 hover:bg-red-100 transition-colors"
                            >
                                <span className="material-symbols-outlined">logout</span>
                                Logout
                            </button>
                        </>
                    )}
                </div>
            </div>
        </ModalOverlay>
    );
};

export default ProfileModal;
