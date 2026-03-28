/**
 * MFAVerify Component
 * 
 * Prompts user to enter MFA code during login
 */

import { useState } from 'react';
import { useMFA } from '../hooks/useMFA';


interface MFAVerifyProps {
    factorId: string;
    onSuccess?: () => void;
    onCancel?: () => void;
}

export function MFAVerify({ factorId, onSuccess, onCancel }: MFAVerifyProps) {
    const { verifyLoginCode, isLoading } = useMFA();
    const [code, setCode] = useState('');
    const [error, setError] = useState<string | null>(null);

    const handleVerify = async () => {
        if (code.length !== 6) {
            setError('Please enter a 6-digit code');
            return;
        }

        setError(null);
        try {
            await verifyLoginCode(code, factorId);
            onSuccess?.();
        } catch (err: unknown) {
            setError('Invalid code. Please try again.');
            setCode('');
        }
    };

    const handleKeyPress = (e: React.KeyboardEvent) => {
        if (e.key === 'Enter' && code.length === 6) {
            handleVerify();
        }
    };

    return (
        <div className="mfa-verify max-w-sm mx-auto p-6 bg-white rounded-lg shadow-lg">
            {/* Header */}
            <div className="mb-6 text-center">
                <span className="material-symbols-outlined text-5xl mx-auto mb-3 text-blue-600">shield</span>
                <h2 className="text-2xl font-bold">Two-Factor Authentication</h2>
                <p className="text-text-secondary mt-2">
                    Enter the 6-digit code from your authenticator app
                </p>
            </div>

            {/* Code Input */}
            <div className="mb-4">
                <input
                    type="text"
                    inputMode="numeric"
                    pattern="[0-9]*"
                    maxLength={6}
                    value={code}
                    onChange={(e) => setCode(e.target.value.replace(/\D/g, ''))}
                    onKeyPress={handleKeyPress}
                    placeholder="000000"
                    className="w-full px-4 py-3 text-center text-2xl font-mono border-2 border-border-medium rounded focus:border-blue-500 focus:outline-none"
                    autoFocus
                    disabled={isLoading}
                />
            </div>

            {/* Error Message */}
            {error && (
                <div className="mb-4 p-3 bg-red-50 border border-red-200 rounded text-red-700 text-sm">
                    {error}
                </div>
            )}

            {/* Verify Button */}
            <button
                onClick={handleVerify}
                disabled={code.length !== 6 || isLoading}
                className="w-full px-4 py-3 bg-blue-600 text-white rounded hover:bg-blue-700 disabled:bg-surface-tertiary flex items-center justify-center gap-2"
            >
                {isLoading ? (
                    <>
                        <span className="material-symbols-outlined text-xl animate-spin">progress_activity</span>
                        Verifying...
                    </>
                ) : (
                    'Verify'
                )}
            </button>

            {/* Cancel Option */}
            {onCancel && (
                <button
                    onClick={onCancel}
                    disabled={isLoading}
                    className="mt-3 w-full text-text-secondary hover:text-text-primary text-sm disabled:text-text-muted"
                >
                    Cancel
                </button>
            )}

            {/* Help Text */}
            <div className="mt-6 pt-4 border-t border-border-light text-xs text-text-secondary text-center">
                <p>Lost access to your authenticator?</p>
                <p className="mt-1">Contact your administrator for assistance.</p>
            </div>
        </div>
    );
}
