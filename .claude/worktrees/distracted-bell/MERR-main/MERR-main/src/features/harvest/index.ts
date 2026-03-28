/**
 * features/harvest — Domain barrel export
 * 
 * Everything related to the harvest process: QR scanning, bucket recording,
 * bin management, and rate limiting.
 */

// Services
export { bucketLedgerService } from '@/services/bucket-ledger.service';
export { binService } from '@/services/bin.service';

// Types
export type { BucketEvent, Bin } from '@/types';

// Schemas (Zod validation)
export { QRPayloadSchema } from '@/lib/schemas';
export type { QRPayload } from '@/lib/schemas';

// Hooks
export { useScanRateLimit } from '@/hooks/useScanRateLimit';
