/**
 * useMFA Hook Tests — initial state and error handling
 */
import { describe, it, expect, vi } from 'vitest';
import { renderHook } from '@testing-library/react';
import { useMFA } from './useMFA';

// Mock supabase auth MFA
vi.mock('../services/supabase', () => ({
    supabase: {
        auth: {
            mfa: {
                enroll: vi.fn(),
                challenge: vi.fn(),
                verify: vi.fn(),
                listFactors: vi.fn(),
                unenroll: vi.fn(),
                getAuthenticatorAssuranceLevel: vi.fn(),
            },
        },
    },
}));

describe('useMFA', () => {
    it('initializes with null state', () => {
        const { result } = renderHook(() => useMFA());
        expect(result.current.qrCode).toBeNull();
        expect(result.current.secret).toBeNull();
        expect(result.current.factorId).toBeNull();
        expect(result.current.isLoading).toBe(false);
        expect(result.current.error).toBeNull();
    });

    it('exposes all action functions', () => {
        const { result } = renderHook(() => useMFA());
        expect(typeof result.current.setupMFA).toBe('function');
        expect(typeof result.current.verifySetupCode).toBe('function');
        expect(typeof result.current.verifyLoginCode).toBe('function');
        expect(typeof result.current.checkMFAStatus).toBe('function');
        expect(typeof result.current.unenrollMFA).toBe('function');
        expect(typeof result.current.getAuthenticatorLevel).toBe('function');
    });
});
