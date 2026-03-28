/**
 * useRunnerManagement Hook
 * Manages Bucket Runners with Persistence (Supabase + Dexie)
 */
import { logger } from '@/utils/logger';
import { useState, useCallback, useMemo, useEffect } from 'react';
import { supabase } from '../services/supabase';
import { db } from '../services/db';

export interface RunnerData {
    id: string;
    name: string;
    avatar: string;
    startTime: string;
    status: 'Active' | 'Break' | 'Off Duty';
    bucketsHandled: number;
    binsCompleted: number;
    currentRow?: number;
}

interface UseRunnerManagementReturn {
    runners: RunnerData[];
    isLoading: boolean;
    addRunner: (runner: Omit<RunnerData, 'id'>) => Promise<void>;
    updateRunner: (runner: RunnerData) => void;
    deleteRunner: (id: string) => void;
    getRunnerById: (id: string) => RunnerData | undefined;
    activeRunners: number;
    totalBucketsHandled: number;
    totalBinsCompleted: number;
}

export const useRunnerManagement = (): UseRunnerManagementReturn => {
    const [runners, setRunners] = useState<RunnerData[]>([]);
    const [isLoading, setIsLoading] = useState(true);

    // Load Runners (Online/Offline)
    useEffect(() => {
        const loadRunners = async () => {
            setIsLoading(true);
            try {
                // 1. Try Cache First
                const cached = await db.runners_cache.toArray();
                if (cached.length > 0) {
                    // Safe cast: cache contains RunnerData objects
                    setRunners(cached as RunnerData[]);
                }

                // 2. Try Online
                if (navigator.onLine) {
                    const { data, error } = await supabase
                        .from('users') // Assuming runners are in users table
                        .select('*')
                        .eq('role', 'bucket_runner'); // Or 'runner'

                    if (!error && data) {
                        const mappedRunners: RunnerData[] = data.map((u: Record<string, string>) => ({
                            id: u.id,
                            name: u.full_name || 'Unknown',
                            avatar: (u.full_name || '??').substring(0, 2).toUpperCase(),
                            startTime: '08:00 AM', // Placeholder until Shift table exists
                            status: 'Active',
                            bucketsHandled: 0,
                            binsCompleted: 0
                        }));

                        setRunners(mappedRunners);
                        // Update UI and Cache
                        await db.runners_cache.clear();
                        await db.runners_cache.bulkPut(mappedRunners);
                    }
                }
            } catch (e) {

                logger.error("Failed to load runners", e);
            } finally {
                setIsLoading(false);
            }
        };
        loadRunners();
    }, []);

    // Add new runner (Simulated 'Add User' for now, or invite)
    const addRunner = useCallback(async (runnerData: Omit<RunnerData, 'id'>) => {
        // In a real app, this would invite a user via email. 
        // For now, we'll create a local record that syncs if possible.
        const newRunner: RunnerData = {
            ...runnerData,
            id: Math.random().toString(36).substring(2, 11) // Temp ID
        };

        setRunners(prev => [...prev, newRunner]);
        await db.runners_cache.put(newRunner);
    }, []);

    const updateRunner = useCallback(async (updatedRunner: RunnerData) => {
        setRunners(prev => prev.map(r => r.id === updatedRunner.id ? updatedRunner : r));
        await db.runners_cache.put(updatedRunner);
    }, []);

    const deleteRunner = useCallback(async (id: string) => {
        setRunners(prev => prev.filter(r => r.id !== id));
        await db.runners_cache.delete(id);
    }, []);

    const getRunnerById = useCallback((id: string): RunnerData | undefined => {
        return runners.find(r => r.id === id);
    }, [runners]);

    const activeRunners = useMemo(() => runners.filter(r => r.status === 'Active').length, [runners]);
    const totalBucketsHandled = useMemo(() => runners.reduce((sum, r) => sum + r.bucketsHandled, 0), [runners]);
    const totalBinsCompleted = useMemo(() => runners.reduce((sum, r) => sum + r.binsCompleted, 0), [runners]);

    return {
        runners,
        isLoading,
        addRunner,
        updateRunner,
        deleteRunner,
        getRunnerById,
        activeRunners,
        totalBucketsHandled,
        totalBinsCompleted
    };
};

export default useRunnerManagement;
