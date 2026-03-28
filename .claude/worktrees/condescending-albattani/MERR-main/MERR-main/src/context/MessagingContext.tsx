/* eslint-disable react-refresh/only-export-components */
/**
 * MessagingContext - Real-time Messaging and Broadcast Management
 * 
 * **Architecture**: React Context API 
 * **Why Context?**: Complex real-time subscriptions, moderate update frequency
 * **Features**: Offline queueing, real-time notifications, broadcast system
 * **See**: `docs/architecture/state-management.md` for decision rationale
 * 
 * @module context/MessagingContext
 * @see {@link file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/docs/architecture/state-management.md}
 */
import React, { createContext, useContext, useState, useRef, useEffect, ReactNode } from 'react';
import { supabase } from '../services/supabase';
import { db } from '../services/db'; // Direct DB access for queue
import { Broadcast, Role, MessagePriority } from '../types';
import { nowNZST } from '@/utils/nzst';
import { logger } from '@/utils/logger';
import { messagingRepository } from '@/repositories/messaging.repository';
import { userRepository2 } from '@/repositories/user.repository';

// Types extracted to messaging.types.ts for reuse
import type { DBMessage, ChatGroup, MessagingState, MessagingContextType } from './messaging.types';
export type { DBMessage, ChatGroup } from './messaging.types';

// =============================================
// CONTEXT
// =============================================
const MessagingContext = createContext<MessagingContextType | undefined>(undefined);

// =============================================
// PROVIDER
// =============================================
export const MessagingProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
    const [state, setState] = useState<MessagingState>({
        messages: [],
        broadcasts: [],
        chatGroups: [],
        unreadCount: 0,
    });

    const userIdRef = useRef<string | null>(null);
    const orchardIdRef = useRef<string | null>(null);
    const subscriptionRef = useRef<ReturnType<typeof supabase.channel> | null>(null);

    const setUserId = (id: string) => {
        userIdRef.current = id;
    };

    const setOrchardId = (id: string) => {
        orchardIdRef.current = id;
    };

    // =============================================
    // MESSAGE ACTIONS
    // =============================================
    const sendMessage = async (
        conversationId: string,
        content: string,
        priority: MessagePriority = 'normal'
    ): Promise<DBMessage | null> => {
        if (!userIdRef.current) {
            logger.error('[MessagingContext] No user ID set');
            return null;
        }

        const tempId = Math.random().toString(36).substring(2, 11);
        const timestamp = nowNZST();

        // 1. Optimistic UI Update
        const optimisticMsg: DBMessage = {
            id: tempId,
            sender_id: userIdRef.current,
            content,
            priority,
            read_by: [userIdRef.current],
            created_at: timestamp,
            orchard_id: orchardIdRef.current || undefined,
            conversation_id: conversationId // Ensure this matches DB format
        } as DBMessage;

        setState(prev => ({
            ...prev,
            messages: [optimisticMsg, ...prev.messages],
        }));

        try {
            // 2. Try Online Send
            if (navigator.onLine) {
                const data = await messagingRepository.sendMessage(conversationId, userIdRef.current, content);

                if (!data) throw new Error('Send failed');

                // Update Conversation updated_at
                await messagingRepository.updateConversationTimestamp(conversationId);

                return data;
            } else {
                throw new Error("Offline");
            }
        } catch (error) {
            logger.warn('[MessagingContext] Offline/Error, queuing message...', error);

            // 3. Fallback to Offline Queue
            await db.message_queue.add({
                id: tempId, // Add ID
                channel_type: 'direct', // Default or derived
                recipient_id: conversationId,
                sender_id: userIdRef.current,
                content,
                timestamp,
                synced: 0,
                priority
            });

            return optimisticMsg;
        }
    };

    const sendBroadcast = async (
        title: string,
        content: string,
        priority: MessagePriority = 'normal',
        targetRoles?: Role[]
    ) => {
        if (!userIdRef.current || !orchardIdRef.current) return;

        try {
            const broadcast: Broadcast = {
                id: Math.random().toString(36).substring(2, 11),
                orchard_id: orchardIdRef.current,
                sender_id: userIdRef.current,
                title,
                content,
                priority,
                target_roles: targetRoles || [Role.TEAM_LEADER, Role.RUNNER],
                acknowledged_by: [],
                created_at: nowNZST(),
            };

            await messagingRepository.insertBroadcast(broadcast as unknown as Record<string, unknown>);

            setState(prev => ({
                ...prev,
                broadcasts: [broadcast, ...prev.broadcasts],
            }));
        } catch (error) {
            logger.error('[MessagingContext] Error sending broadcast:', error);
        }
    };

    const getOrCreateConversation = async (participantId: string): Promise<string | null> => {
        if (!userIdRef.current) return null;

        try {
            // Find direct conversation with exactly these participants
            const existing = await messagingRepository.findDirectConversation(userIdRef.current, participantId);

            // Filter locally to ensure ONLY these two participants (Supabase 'contains' might match 3+)
            const match = existing?.[0];

            if (match) return match.id;

            // Create new direct conversation
            const newConv = await messagingRepository.createConversation('direct', [userIdRef.current, participantId], userIdRef.current);

            if (!newConv) throw new Error('Failed to create conversation');
            return newConv.id;
        } catch (error) {
            logger.error('[MessagingContext] Error getOrCreateConversation:', error);
            return null;
        }
    };

    const markMessageRead = async (messageId: string) => {
        if (!userIdRef.current) return;

        setState(prev => ({
            ...prev,
            messages: prev.messages.map(m =>
                m.id === messageId && !m.read_by.includes(userIdRef.current!)
                    ? { ...m, read_by: [...m.read_by, userIdRef.current!] }
                    : m
            ),
            unreadCount: Math.max(0, prev.unreadCount - 1),
        }));
    };

    const acknowledgeBroadcast = async (broadcastId: string) => {
        if (!userIdRef.current) return;

        setState(prev => ({
            ...prev,
            broadcasts: prev.broadcasts.map(b =>
                b.id === broadcastId && !b.acknowledged_by.includes(userIdRef.current!)
                    ? { ...b, acknowledged_by: [...b.acknowledged_by, userIdRef.current!] }
                    : b
            ),
        }));
    };

    // =============================================
    // CHAT GROUPS
    // =============================================
    const createChatGroup = async (name: string, memberIds: string[]): Promise<ChatGroup | null> => {
        if (!userIdRef.current) return null;

        try {
            const allParticipants = [...new Set([userIdRef.current, ...memberIds])];

            const { data, error } = await messagingRepository.createGroupConversation(
                name, allParticipants, userIdRef.current
            );

            if (error) throw error;

            const group: ChatGroup = {
                id: data.id,
                name: data.name,
                members: data.participant_ids,
                isGroup: true,
                lastMsg: 'Group created',
                time: new Date().toLocaleTimeString('en-NZ', { hour: '2-digit', minute: '2-digit' }),
            };

            setState(prev => ({
                ...prev,
                chatGroups: [group, ...prev.chatGroups],
            }));

            return group;
        } catch (error) {
            logger.error('[MessagingContext] Error creating group:', error);
            throw error;
        }
    };

    const loadChatGroups = async () => {
        // In a real app, load from Supabase
        // For now, just return empty - groups are managed locally
    };

    const loadConversation = async (conversationId: string): Promise<DBMessage[]> => {
        try {
            const data = await messagingRepository.getConversationMessages(conversationId);
            return data || [];
        } catch (error) {
            logger.error('[MessagingContext] Error loading conversation:', error);
            return [];
        }
    };

    const refreshMessages = async () => {
        if (!orchardIdRef.current || !userIdRef.current) return;
        const currentUserId = userIdRef.current;

        try {
            // 1. Load Broadcasts
            const broadcastsData = await messagingRepository.getBroadcasts(orchardIdRef.current);

            // 2. Load Conversations
            const convData = await messagingRepository.getConversations(currentUserId);

            // 3. Collect unique participant IDs to resolve names
            const participantIds = new Set<string>();
            (convData || []).forEach(c => {
                (c.participant_ids || []).forEach((pid: string) => {
                    if (pid !== currentUserId) participantIds.add(pid);
                });
            });

            // 4. Fetch profiles for all participants in one batch
            let profileMap: Record<string, string> = {};
            if (participantIds.size > 0) {
                profileMap = await userRepository2.getNamesByIds(Array.from(participantIds));
            }

            // 5. Fetch last message for each conversation
            const convIds = (convData || []).map(c => c.id);
            const lastMsgMap: Record<string, { content: string; created_at: string }> = {};
            if (convIds.length > 0) {
                const lastMsgs = await messagingRepository.getLastMessages(convIds);
                if (lastMsgs) {
                    lastMsgs.forEach((m: { conversation_id: string; content: string; created_at: string }) => {
                        if (!lastMsgMap[m.conversation_id]) {
                            lastMsgMap[m.conversation_id] = { content: m.content, created_at: m.created_at };
                        }
                    });
                }
            }

            // 6. Build chat groups with resolved names
            const chatGroups: ChatGroup[] = (convData || []).map(c => {
                let displayName = c.name;
                const isDirect = c.type !== 'group';

                if (isDirect || !displayName) {
                    // For DMs: show the other participant's name
                    const otherIds = (c.participant_ids || []).filter((pid: string) => pid !== currentUserId);
                    const otherNames = otherIds.map((pid: string) => profileMap[pid] || 'Unknown');
                    displayName = otherNames.length > 0 ? otherNames.join(', ') : 'Direct Chat';
                }

                const lastMsg = lastMsgMap[c.id];
                const timeStr = lastMsg
                    ? new Date(lastMsg.created_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
                    : c.updated_at
                        ? new Date(c.updated_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
                        : '';

                return {
                    id: c.id,
                    name: displayName,
                    members: c.participant_ids,
                    isGroup: c.type === 'group',
                    lastMsg: lastMsg ? (lastMsg.content.length > 40 ? lastMsg.content.substring(0, 40) + '…' : lastMsg.content) : '',
                    time: timeStr,
                    unreadCount: 0,
                };
            });

            setState(prev => ({
                ...prev,
                broadcasts: broadcastsData || [],
                chatGroups,
            }));
        } catch (error) {
            logger.error('[MessagingContext] Error refreshing messages:', error);
        }
    };

    // =============================================
    // CLEANUP
    // =============================================
    useEffect(() => {
        const currentSub = subscriptionRef.current;
        return () => {
            if (currentSub) {
                currentSub.unsubscribe();
            }
        };
    }, []);

    // =============================================
    // REALTIME UPDATES
    // =============================================
    useEffect(() => {
        if (!orchardIdRef.current) return;

        const channel = supabase
            .channel('public:broadcasts')
            .on(
                'postgres_changes',
                {
                    event: 'INSERT',
                    schema: 'public',
                    table: 'broadcasts',
                    filter: `orchard_id=eq.${orchardIdRef.current}` // Filter by orchard
                },
                (payload) => {
                    const newBroadcast = payload.new as Broadcast;

                    // 1. Update State
                    setState(prev => {
                        // Avoid duplicates
                        if (prev.broadcasts.some(b => b.id === newBroadcast.id)) return prev;
                        return {
                            ...prev,
                            broadcasts: [newBroadcast, ...prev.broadcasts]
                        };
                    });

                    // 2. TRIGGER WAKE-UP (Haptic + Sound)
                    // Only if it's not our own message (optional, but good for confirmation too)
                    // if (newBroadcast.sender_id !== userIdRef.current) {
                    try {
                        // Vibrate pattern: Pulse-Pulse-Long
                        if (navigator.vibrate) {
                            navigator.vibrate([200, 100, 200, 100, 500]);
                        }

                        // Audio Feedback (Simple Beep via AudioContext or HTML5 Audio)
                        // Ideally use a file, but for now we rely on vibration primarily
                        // or a system notification if PWA is installed.

                        // System Notification (if permission granted)
                        if (Notification.permission === 'granted') {
                            new Notification(`📢 ${newBroadcast.title}`, {
                                body: newBroadcast.content,
                                icon: '/pwa-192x192.png' // Ensure this exists
                            });
                        }
                    } catch (e) {
                        logger.warn('[MessagingContext] Feedback trigger failed', e);
                    }
                    // }
                }
            )
            .subscribe((status) => {
                if (status === 'SUBSCRIBED') {
                    logger.debug('[MessagingContext] Broadcast subscription active');
                }
            });

        return () => {
            supabase.removeChannel(channel);
        };
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [orchardIdRef.current]);  // Re-subscribe if orchard changes

    // =============================================
    // CONTEXT VALUE
    // =============================================
    const contextValue: MessagingContextType = {
        ...state,
        sendMessage,
        sendBroadcast,
        getOrCreateConversation,
        markMessageRead,
        acknowledgeBroadcast,
        createChatGroup,
        loadChatGroups,
        loadConversation,
        refreshMessages,
        setOrchardId,
        setUserId,
    };

    return <MessagingContext.Provider value={contextValue}>{children}</MessagingContext.Provider>;
};

// =============================================
// HOOK
// =============================================
export const useMessaging = (): MessagingContextType => {
    const context = useContext(MessagingContext);
    if (!context) {
        throw new Error('useMessaging must be used within a MessagingProvider');
    }
    return context;
};

export default MessagingContext;