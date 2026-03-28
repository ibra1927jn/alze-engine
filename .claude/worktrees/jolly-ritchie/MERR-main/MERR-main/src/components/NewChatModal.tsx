/**
 * NewChatModal — Create direct or group conversations
 * Extracted from SimpleChat.tsx for modularity.
 */
import { useState } from 'react';

export interface NewChatModalProps {
    users: Array<{ id: string; name: string; role: string }>;
    onClose: () => void;
    onCreate: (type: 'direct' | 'group', participantIds: string[], name?: string) => void;
}

const NewChatModal = ({ users, onClose, onCreate }: NewChatModalProps) => {
    const [chatType, setChatType] = useState<'direct' | 'group'>('direct');
    const [selectedUsers, setSelectedUsers] = useState<string[]>([]);
    const [groupName, setGroupName] = useState('');

    const toggleUser = (id: string) => {
        if (chatType === 'direct') {
            setSelectedUsers([id]);
        } else {
            setSelectedUsers(prev =>
                prev.includes(id) ? prev.filter(u => u !== id) : [...prev, id]
            );
        }
    };

    const handleCreate = () => {
        if (selectedUsers.length === 0) return;
        if (chatType === 'group' && !groupName.trim()) return;

        onCreate(chatType, selectedUsers, groupName.trim() || undefined);
    };

    return (
        <div className="fixed inset-0 z-[100] flex items-center justify-center bg-black/70 backdrop-blur-sm" onClick={onClose}>
            <div className="bg-surface-white rounded-3xl p-6 w-[90%] max-w-lg shadow-2xl border border-border-light" onClick={e => e.stopPropagation()}>
                <div className="flex items-center justify-between mb-6">
                    <h3 className="text-xl font-black text-text-main">New Chat</h3>
                    <button onClick={onClose} className="text-text-muted hover:text-text-main">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                {/* Chat Type Toggle */}
                <div className="flex gap-2 mb-4">
                    <button
                        onClick={() => { setChatType('direct'); setSelectedUsers([]); }}
                        className={`flex-1 py-2 rounded-xl font-bold transition ${chatType === 'direct' ? 'bg-primary text-white' : 'bg-slate-100 text-text-muted'
                            }`}
                    >
                        Direct
                    </button>
                    <button
                        onClick={() => { setChatType('group'); setSelectedUsers([]); }}
                        className={`flex-1 py-2 rounded-xl font-bold transition ${chatType === 'group' ? 'bg-primary text-white' : 'bg-slate-100 text-text-muted'
                            }`}
                    >
                        Group
                    </button>
                </div>

                {/* Group Name (only for groups) */}
                {chatType === 'group' && (
                    <input
                        type="text"
                        value={groupName}
                        onChange={(e) => setGroupName(e.target.value)}
                        placeholder="Group name"
                        className="w-full bg-slate-100 border border-border-light rounded-xl px-4 py-3 text-text-main focus:border-primary outline-none mb-4"
                    />
                )}

                {/* User List */}
                <p className="text-xs font-bold text-text-muted uppercase mb-2">
                    Select {chatType === 'direct' ? 'User' : 'Members'} ({selectedUsers.length})
                </p>
                <div className="space-y-2 max-h-60 overflow-y-auto mb-4">
                    {users.map(user => (
                        <label
                            key={user.id}
                            className={`flex items-center gap-3 p-3 rounded-lg cursor-pointer transition ${selectedUsers.includes(user.id) ? 'bg-primary/20 border border-primary/50' : 'bg-slate-50 border border-border-light'
                                }`}
                        >
                            <input
                                type={chatType === 'direct' ? 'radio' : 'checkbox'}
                                checked={selectedUsers.includes(user.id)}
                                onChange={() => toggleUser(user.id)}
                                className="size-5 accent-primary"
                            />
                            <div>
                                <p className="font-bold text-text-main text-sm">{user.name}</p>
                                <p className="text-xs text-text-muted">{user.role}</p>
                            </div>
                        </label>
                    ))}
                </div>

                {/* Create Button */}
                <button
                    onClick={handleCreate}
                    disabled={selectedUsers.length === 0 || (chatType === 'group' && !groupName.trim())}
                    className="w-full py-4 bg-primary text-white rounded-xl font-bold uppercase disabled:bg-slate-300 transition"
                >
                    {chatType === 'direct' ? 'Start Chat' : 'Create Group'}
                </button>
            </div>
        </div>
    );
};

export default NewChatModal;
