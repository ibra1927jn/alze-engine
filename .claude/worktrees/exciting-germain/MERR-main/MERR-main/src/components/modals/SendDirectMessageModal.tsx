/**
 * Send Direct Message Modal - Shared component
 */
import React, { useState } from 'react';
import ModalOverlay from '@/components/ui/ModalOverlay';

export interface Recipient {
    id: string;
    name: string;
    role: string;
    department?: string;
}

interface SendDirectMessageModalProps {
    onClose: () => void;
    onSend: (recipient: Recipient, message: string) => void;
    recipients: Recipient[];
    accentColor?: string;
}

const SendDirectMessageModal: React.FC<SendDirectMessageModalProps> = ({
    onClose,
    onSend,
    recipients,
    accentColor: _accentColor,
}) => {
    const [selectedRecipient, setSelectedRecipient] = useState<Recipient | null>(null);
    const [message, setMessage] = useState('');

    const handleSend = () => {
        if (selectedRecipient && message.trim()) {
            onSend(selectedRecipient, message);
            onClose();
        }
    };

    return (
        <ModalOverlay onClose={onClose}>
            <div className="p-6 max-h-[85vh] overflow-y-auto">
                <div className="flex items-center justify-between mb-6">
                    <h3 className="text-xl font-black text-text-main">Send Direct Message</h3>
                    <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                <p className="text-xs font-bold text-text-muted uppercase mb-3">Select Recipient</p>
                <div className="space-y-2 mb-4 max-h-60 overflow-y-auto">
                    {recipients.map(person => (
                        <label
                            key={person.id}
                            className={`flex items-center gap-3 p-3 rounded-lg cursor-pointer transition-all ${selectedRecipient?.id === person.id
                                ? 'bg-primary text-white'
                                : 'bg-slate-50 hover:bg-slate-100'
                                }`}
                        >
                            <input
                                type="radio"
                                name="recipient"
                                checked={selectedRecipient?.id === person.id}
                                onChange={() => setSelectedRecipient(person)}
                                className="size-5 accent-primary"
                            />
                            <div className="flex-1">
                                <p
                                    className={`font-bold text-sm ${selectedRecipient?.id === person.id ? 'text-white' : 'text-text-main'
                                        }`}
                                >
                                    {person.name}
                                </p>
                                <p
                                    className={`text-xs ${selectedRecipient?.id === person.id ? 'text-white/80' : 'text-text-muted'
                                        }`}
                                >
                                    {person.role}
                                    {person.department && ` • ${person.department}`}
                                </p>
                            </div>
                        </label>
                    ))}
                </div>

                <p className="text-xs font-bold text-text-muted uppercase mb-2">Your Message</p>
                <textarea
                    value={message}
                    onChange={e => setMessage(e.target.value)}
                    placeholder="Type your message here..."
                    className="w-full px-4 py-3 rounded-xl border-2 border-border-light focus:border-primary outline-none resize-none mb-4 transition-colors"
                    rows={4}
                />

                <button
                    onClick={handleSend}
                    disabled={!selectedRecipient || !message.trim()}
                    className="w-full py-4 gradient-primary glow-primary text-white rounded-xl font-bold uppercase tracking-widest disabled:bg-surface-tertiary disabled:shadow-none active:scale-95 transition-all flex items-center justify-center gap-2"
                >
                    <span className="material-symbols-outlined">send</span>
                    Send Message
                </button>
            </div>
        </ModalOverlay>
    );
};

export default SendDirectMessageModal;
