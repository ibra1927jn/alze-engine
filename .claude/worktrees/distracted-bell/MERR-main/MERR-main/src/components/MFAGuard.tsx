/**
 * MFAGuard Component
 * 
 * Wrapper that enforces MFA setup for manager role
 * Shows MFASetup modal if manager doesn't have MFA enabled
 */

import { logger } from '@/utils/logger';
import { useEffect, useState } from 'react';
import { useAuth } from '../context/AuthContext';
import { useMFA } from '../hooks/useMFA';
import { MFASetup } from './MFASetup';

interface MFAGuardProps {
    children: React.ReactNode;
}

export function MFAGuard({ children }: MFAGuardProps) {
    const { currentRole, isAuthenticated } = useAuth();
    const { checkMFAStatus } = useMFA();
    const [mfaStatus, setMfaStatus] = useState<{ checked: boolean; hasVerifiedFactor: boolean }>({
        checked: false,
        hasVerifiedFactor: false,
    });
    const [showSetup, setShowSetup] = useState(false);

    useEffect(() => {
        const checkStatus = async () => {
            if (!isAuthenticated || currentRole !== 'manager') {
                setMfaStatus({ checked: true, hasVerifiedFactor: true });
                return;
            }

            try {
                const status = await checkMFAStatus();
                setMfaStatus({
                    checked: true,
                    hasVerifiedFactor: status.hasVerifiedFactor,
                });

                // If manager doesn't have MFA, force setup
                if (!status.hasVerifiedFactor) {
                    setShowSetup(true);
                }
            } catch (error) {
                 
                logger.error('[MFAGuard] Error checking MFA status:', error);
                // On error, allow access (fail open)
                setMfaStatus({ checked: true, hasVerifiedFactor: true });
            }
        };

        checkStatus();
    }, [isAuthenticated, currentRole, checkMFAStatus]);

    const handleMFAComplete = () => {
        setShowSetup(false);
        setMfaStatus({ checked: true, hasVerifiedFactor: true });
    };

    // Still checking
    if (!mfaStatus.checked) {
        return (
            <div className="flex items-center justify-center min-h-screen">
                <div className="text-center">
                    <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                    <p className="text-text-secondary">Checking security settings...</p>
                </div>
            </div>
        );
    }

    // Manager needs to setup MFA
    if (showSetup && currentRole === 'manager') {
        return (
            <div className="flex items-center justify-center min-h-screen bg-background-light">
                <div className="max-w-2xl w-full px-4">
                    <div className="mb-6 bg-blue-50 border border-blue-200 rounded-lg p-4">
                        <h2 className="font-bold text-blue-900 mb-2">🔒 Two-Factor Authentication Required</h2>
                        <p className="text-blue-800 text-sm">
                            As a manager, you must enable two-factor authentication to access the application.
                            This additional security layer protects sensitive data and ensures compliance.
                        </p>
                    </div>
                    <MFASetup
                        onComplete={handleMFAComplete}
                        requireSetup={true}
                    />
                </div>
            </div>
        );
    }

    // MFA verified or not required
    return <>{children}</>;
}
