/**
 * features/logistics — Domain barrel export
 * 
 * Fleet management, transport requests, bin tracking.
 */

// Services (standalone exported functions)
export {
    fetchLogisticsSummary,
    fetchFleet,
    fetchBinInventory,
    fetchTransportRequests,
    fetchTransportHistory,
    createTransportRequest,
    assignVehicleToRequest,
    completeTransportRequest,
} from '@/services/logistics-dept.service';

// Types
export type {
    Tractor,
    BinInventory,
    TransportRequest,
    TransportLog,
    LogisticsSummary,
} from '@/services/logistics-dept.service';
