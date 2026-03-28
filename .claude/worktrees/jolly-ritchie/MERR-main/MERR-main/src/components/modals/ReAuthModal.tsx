/**
 * ReAuthModal - Session Re-authentication Modal
 * 
 * 🔧 R8-Fix2: Prevents the "Dead Session" deadlock.
 * When JWT expires while offline with pending data,
 * this modal lets the user re-authenticate WITHOUT calling signOut()
 * (which would trigger V27 guard and wipe Dexie).
 * 
 * The modal is ineludible — it overlays the entire app until the user
 * successfully re-authenticates with their password.
 */
import React, { useState, useEffect, useCallback } from 'react';
import { supabase } from '@/services/supabase';
import { syncService } from '@/services/sync.service';
import { logger } from '@/utils/logger';

interface ReAuthModalProps {
    /** The email of the expired session user */
    email: string;
    /** Called after successful re-authentication */
    onReAuthenticated: () => void;
}

const ReAuthModal: React.FC<ReAuthModalProps> = ({ email, onReAuthenticated }) => {
    const [password, setPassword] = useState('');
    const [error, setError] = useState('');
    const [isLoading, setIsLoading] = useState(false);
    const [pendingCount, setPendingCount] = useState(0);

    useEffect(() => {
        syncService.getPendingCount().then(setPendingCount);
    }, []);

    const handleReAuth = useCallback(async (e: React.FormEvent) => {
        e.preventDefault();
        setIsLoading(true);
        setError('');

        try {
            const { error: authError } = await supabase.auth.signInWithPassword({
                email,
                password,
            });

            if (authError) {
                setError(authError.message);
                return;
            }

            logger.info('[ReAuthModal] Re-authentication successful. Resuming sync.');
            onReAuthenticated();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'Error inesperado');
        } finally {
            setIsLoading(false);
        }
    }, [email, password, onReAuthenticated]);

    return (
        <div style={{
            position: 'fixed',
            top: 0,
            left: 0,
            width: '100vw',
            height: '100vh',
            backgroundColor: 'rgba(0, 0, 0, 0.85)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 99999,
        }}>
            <div style={{
                backgroundColor: 'var(--color-surface, #1e1e2e)',
                borderRadius: '16px',
                padding: '32px',
                maxWidth: '400px',
                width: '90vw',
                boxShadow: '0 8px 32px rgba(0,0,0,0.5)',
                border: '1px solid var(--color-border, #333)',
            }}>
                <h2 style={{ color: 'var(--color-text, #fff)', marginTop: 0, fontSize: '20px' }}>
                    🔒 Sesión Expirada
                </h2>

                <p style={{ color: 'var(--color-text-muted, #aaa)', fontSize: '14px', lineHeight: '1.5' }}>
                    Tu sesión ha expirado, pero tienes{' '}
                    <strong style={{ color: '#ff6b6b' }}>{pendingCount} registros</strong>{' '}
                    pendientes de sincronización. Ingresa tu contraseña para reconectarte sin perderlos.
                </p>

                <form onSubmit={handleReAuth}>
                    <input
                        type="email"
                        value={email}
                        disabled
                        aria-label="Correo electrónico"
                        style={{
                            width: '100%',
                            padding: '10px 12px',
                            marginBottom: '12px',
                            borderRadius: '8px',
                            border: '1px solid #444',
                            backgroundColor: '#2a2a3e',
                            color: '#888',
                            fontSize: '14px',
                            boxSizing: 'border-box',
                        }}
                    />
                    <input
                        type="password"
                        value={password}
                        onChange={(e) => setPassword(e.target.value)}
                        placeholder="Contraseña"
                        autoFocus
                        required
                        aria-label="Contraseña"
                        style={{
                            width: '100%',
                            padding: '10px 12px',
                            marginBottom: '16px',
                            borderRadius: '8px',
                            border: '1px solid #444',
                            backgroundColor: '#2a2a3e',
                            color: '#fff',
                            fontSize: '14px',
                            boxSizing: 'border-box',
                        }}
                    />

                    {error && (
                        <p style={{ color: '#ff6b6b', fontSize: '13px', margin: '0 0 12px 0' }}>
                            ❌ {error}
                        </p>
                    )}

                    <button
                        type="submit"
                        disabled={isLoading || !password}
                        style={{
                            width: '100%',
                            padding: '12px',
                            borderRadius: '8px',
                            border: 'none',
                            backgroundColor: isLoading ? '#555' : '#4CAF50',
                            color: '#fff',
                            fontSize: '16px',
                            fontWeight: 'bold',
                            cursor: isLoading ? 'not-allowed' : 'pointer',
                        }}
                    >
                        {isLoading ? '⏳ Reconectando...' : '🔓 Reconectar y Sincronizar'}
                    </button>
                </form>

                <p style={{ color: '#666', fontSize: '11px', marginTop: '16px', textAlign: 'center' }}>
                    ⚠️ No cierres esta ventana. Tus datos están seguros en el dispositivo.
                </p>
            </div>
        </div>
    );
};

export default ReAuthModal;
