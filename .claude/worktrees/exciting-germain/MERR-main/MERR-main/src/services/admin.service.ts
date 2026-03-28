/**
 * admin.service.ts — HR/Admin Service
 * 
 * Provides cross-orchard management capabilities for system administrators.
 * Includes multi-orchard overview, user management, and compliance stats.
 */
import { logger } from '@/utils/logger';
import { adminRepository } from '@/repositories/admin.repository';

export interface OrchardOverview {
    id: string;
    name: string;
    total_rows: number;
    active_pickers: number;
    today_buckets: number;
    compliance_score: number;
}

export interface UserRecord {
    id: string;
    email: string;
    full_name: string;
    role: string;
    is_active: boolean;
    orchard_id: string | null;
    orchard_name?: string;
    created_at: string;
}

export const adminService = {
    /**
     * Fetch all orchards with aggregated stats
     */
    async getAllOrchards(): Promise<OrchardOverview[]> {
        try {
            return await adminRepository.getAllOrchards();
        } catch (error) {
            logger.error('[AdminService] Failed to fetch orchards:', (error as Error).message);
            return [];
        }
    },

    /**
     * Fetch all users with optional role filter
     */
    async getAllUsers(filters?: {
        role?: string;
        orchardId?: string;
        search?: string;
    }): Promise<UserRecord[]> {
        try {
            return await adminRepository.getAllUsers(filters);
        } catch (error) {
            logger.error('[AdminService] Failed to fetch users:', (error as Error).message);
            return [];
        }
    },

    /**
     * Update a user's role
     */
    async updateUserRole(userId: string, newRole: string): Promise<boolean> {
        try {
            await adminRepository.updateUserRole(userId, newRole);
            return true;
        } catch (error) {
            logger.error('[AdminService] Failed to update user role:', (error as Error).message);
            return false;
        }
    },

    /**
     * Deactivate a user (soft-delete)
     */
    async deactivateUser(userId: string): Promise<boolean> {
        try {
            await adminRepository.deactivateUser(userId);
            return true;
        } catch (error) {
            logger.error('[AdminService] Failed to deactivate user:', (error as Error).message);
            return false;
        }
    },

    /**
     * Reactivate a user
     */
    async reactivateUser(userId: string): Promise<boolean> {
        try {
            await adminRepository.reactivateUser(userId);
            return true;
        } catch (error) {
            logger.error('[AdminService] Failed to reactivate user:', (error as Error).message);
            return false;
        }
    },
};
