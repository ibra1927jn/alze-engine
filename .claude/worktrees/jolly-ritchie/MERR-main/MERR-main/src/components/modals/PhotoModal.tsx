/**
 * Photo Report Modal - For capturing and sending photo reports
 */
import React, { useState } from 'react';
import { useToast } from '@/hooks/useToast';
import ModalOverlay from '@/components/ui/ModalOverlay';

interface PhotoModalProps {
    onClose: () => void;
    onSend?: (notes: string, hasPhoto: boolean) => void;
}

const PhotoModal: React.FC<PhotoModalProps> = ({ onClose, onSend }) => {
    const [photoTaken, setPhotoTaken] = useState(false);
    const [notes, setNotes] = useState('');
    const { showToast } = useToast();

    const handleCapture = () => {
        setPhotoTaken(true);
        setTimeout(() => setPhotoTaken(false), 2000);
    };

    const handleSend = () => {
        if (photoTaken || notes) {
            onSend?.(notes, photoTaken);
            showToast(`📸 Photo Report Sent! ${notes || 'No notes added'} — Manager and Team Leaders notified`, 'success');
            onClose();
        }
    };

    return (
        <ModalOverlay onClose={onClose} maxWidth="max-w-sm">
            <div className="p-8">
                <div className="flex items-center justify-between mb-6">
                    <h3 className="text-xl font-black text-text-main">Photo Report</h3>
                    <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                <div
                    onClick={handleCapture}
                    className={`rounded-2xl h-64 flex flex-col items-center justify-center mb-6 cursor-pointer transition-all ${photoTaken
                        ? 'bg-green-50 border-2 border-success'
                        : 'bg-slate-50 border-2 border-dashed border-border-light'
                        }`}
                >
                    {photoTaken ? (
                        <>
                            <span className="material-symbols-outlined text-success text-6xl mb-2">
                                check_circle
                            </span>
                            <p className="text-success text-sm font-bold">Photo Captured!</p>
                        </>
                    ) : (
                        <>
                            <span className="material-symbols-outlined text-text-muted text-6xl mb-2">
                                add_a_photo
                            </span>
                            <p className="text-text-muted text-sm font-bold">Tap to capture</p>
                        </>
                    )}
                </div>

                <textarea
                    value={notes}
                    onChange={e => setNotes(e.target.value)}
                    placeholder="Add notes (optional): e.g. 'Damaged bin at Row 12'"
                    className="w-full px-4 py-3 rounded-xl border-2 border-border-light mb-4 focus:border-primary outline-none resize-none transition-colors"
                    rows={3}
                />

                <button
                    onClick={handleSend}
                    disabled={!photoTaken && !notes}
                    className="w-full py-4 gradient-primary glow-primary text-white rounded-xl font-bold uppercase tracking-widest active:scale-95 transition-all disabled:bg-surface-tertiary disabled:shadow-none"
                >
                    Send Report
                </button>
            </div>
        </ModalOverlay>
    );
};

export default PhotoModal;
