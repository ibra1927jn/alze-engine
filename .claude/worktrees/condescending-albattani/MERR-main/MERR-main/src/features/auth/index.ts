/**
 * features/auth — Domain barrel export
 * 
 * Authentication, login hardening, MFA, user management.
 */

// Services
export { authHardeningService } from '@/services/authHardening.service';
export { userService } from '@/services/user.service';

// Sentry context
export { setSentryUser, clearSentryUser } from '@/config/sentry';
