
import { logger } from '@/utils/logger';
import { messagingRepository } from '@/repositories/messaging.repository';
import { userRepository2 } from '@/repositories/user.repository';

export interface ChatMessage {
    id: string;
    conversation_id: string;
    sender_id: string;
    content: string;
    created_at: string;
    sender_name?: string;
    sender?: { full_name: string };
}

export interface Conversation {
    id: string;
    type: 'direct' | 'group';
    name?: string;
    participant_ids: string[];
    last_message?: string;
    updated_at: string;
}

export const simpleMessagingService = {

    async sendMessage(conversationId: string, senderId: string, content: string) {
        return messagingRepository.sendMessage(conversationId, senderId, content);
    },

    async getMessages(conversationId: string) {
        const data = await messagingRepository.getMessages(conversationId);
        return (data || []).map((msg: unknown) => {
            const m = msg as Record<string, unknown>;
            return {
                id: String(m.id),
                conversation_id: String(m.conversation_id),
                sender_id: String(m.sender_id),
                content: String(m.content),
                created_at: String(m.created_at),
                sender_name: (m.sender as Record<string, unknown>)?.full_name as string || 'Unknown'
            } as ChatMessage;
        });
    },

    subscribeToConversation(conversationId: string, onMessage: (msg: ChatMessage) => void) {
        return messagingRepository.subscribe(conversationId, (payload) => {
            onMessage(payload as ChatMessage);
        });
    },

    async getConversations(userId: string): Promise<Conversation[]> {
        try {
            const data = await messagingRepository.getConversations(userId);
            return data as Conversation[];
        } catch (e) {
            logger.warn("Failed to fetch conversations", e);
            return [];
        }
    },

    async getUsers() {
        const data = await userRepository2.getAll();
        return data.map(u => ({
            id: u.id,
            name: u.full_name || 'Unknown',
            role: u.role || 'picker'
        }));
    },

    async createConversation(type: 'direct' | 'group', participantIds: string[], createdBy: string, name?: string) {
        return messagingRepository.createConversation(type, participantIds, createdBy, name) as Promise<Conversation | null>;
    }
};
